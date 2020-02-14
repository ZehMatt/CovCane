#include "Rewriter.h"
#include "Logging.h"
#include "Translation.h"
#include "Runtime.h"

#include <unordered_map>
#include <mutex>

namespace CovCane {

static Runtime _jitRT;
static std::unordered_map<uintptr_t, uintptr_t> _sourceToTarget;
static std::unordered_map<uintptr_t, uintptr_t> _targetToSource;
static std::mutex _lock;

using DecodedBranch = std::vector<ZydisDecodedInstruction>;

static void PrintBranchInstructions(const DecodedBranch& decoded)
{
    ZydisFormatter fmt;
    ZydisFormatterInit(&fmt, ZYDIS_FORMATTER_STYLE_INTEL);
    for (auto& ins : decoded)
    {
        char buffer[128]{};
        ZydisFormatterFormatInstruction(&fmt, &ins, buffer, sizeof(buffer));
        Logging::Msg("0x%p %s", ins.instrAddress, buffer);
    }
}

bool Rewriter::CreateSectionBuffer(uintptr_t startVA, uintptr_t endVA)
{
    return _jitRT.createBuffer(startVA, endVA) != nullptr;
}

static bool IsDirectCondControlFlow(const ZydisDecodedInstruction& ins)
{
    switch (ins.mnemonic)
    {
        case ZYDIS_MNEMONIC_JB:
        case ZYDIS_MNEMONIC_JBE:
        case ZYDIS_MNEMONIC_JCXZ:
        case ZYDIS_MNEMONIC_JECXZ:
        case ZYDIS_MNEMONIC_JKNZD:
        case ZYDIS_MNEMONIC_JKZD:
        case ZYDIS_MNEMONIC_JL:
        case ZYDIS_MNEMONIC_JLE:
        case ZYDIS_MNEMONIC_JNB:
        case ZYDIS_MNEMONIC_JNBE:
        case ZYDIS_MNEMONIC_JNL:
        case ZYDIS_MNEMONIC_JNLE:
        case ZYDIS_MNEMONIC_JNO:
        case ZYDIS_MNEMONIC_JNP:
        case ZYDIS_MNEMONIC_JNS:
        case ZYDIS_MNEMONIC_JNZ:
        case ZYDIS_MNEMONIC_JO:
        case ZYDIS_MNEMONIC_JP:
        case ZYDIS_MNEMONIC_JRCXZ:
        case ZYDIS_MNEMONIC_JS:
        case ZYDIS_MNEMONIC_JZ:
            return true;
    }
    return false;
}

static bool IsDirectControlFlow(const ZydisDecodedInstruction& ins)
{
    if (IsDirectCondControlFlow(ins))
        return true;
    switch (ins.mnemonic)
    {
        case ZYDIS_MNEMONIC_JMP:
            return true;
    }
    return false;
}

static bool IsBranchTerminal(const ZydisDecodedInstruction& ins)
{
    switch (ins.mnemonic)
    {
        case ZYDIS_MNEMONIC_CALL:
        case ZYDIS_MNEMONIC_JMP:
        case ZYDIS_MNEMONIC_RET:
        case ZYDIS_MNEMONIC_IRET:
        case ZYDIS_MNEMONIC_IRETD:
        case ZYDIS_MNEMONIC_IRETQ:
        case ZYDIS_MNEMONIC_INT3:
        case ZYDIS_MNEMONIC_SYSCALL:
            return true;
    }
    return false;
}

static DecodedBranch DecodeBranch(
    uintptr_t source, bool rewrittenBranch = false)
{
    DecodedBranch decoded;

    ZydisDecoder decoder;
#ifdef _M_X64
    ZydisDecoderInit(
        &decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);
#else
    ZydisDecoderInit(
        &decoder, ZYDIS_MACHINE_MODE_LONG_COMPAT_32, ZYDIS_ADDRESS_WIDTH_32);
#endif

    bool hasPushRax = false;
    bool hasMov = false;
    bool hasXchg = false;

    for (uintptr_t va = source;;)
    {
        ZydisDecodedInstruction* ins = &decoded.emplace_back();

        auto status = ZydisDecoderDecodeBuffer(
            &decoder, reinterpret_cast<const void*>(va), 16, va, ins);
        if (status != ZYDIS_STATUS_SUCCESS)
        {
            Logging::Msg("Unable to decode instruction at %p", va);

            // Destroy last.
            decoded.pop_back();

            break;
        }

        if (ins->mnemonic == ZYDIS_MNEMONIC_RET
            && ins->operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE
            && ins->operands[0].imm.value.u == 0)
        {
            ins->operandCount = 0;
            ins->operands[0].type = ZYDIS_OPERAND_TYPE_UNUSED;
        }

        if (rewrittenBranch)
        {
            if (ins->mnemonic == ZYDIS_MNEMONIC_PUSH
                && ins->operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER
                && ins->operands[0].reg.value == ZYDIS_REGISTER_RAX)
            {
                hasPushRax = true;
            }
            else if (
                ins->mnemonic == ZYDIS_MNEMONIC_MOV && hasPushRax
                && ins->operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER
                && ins->operands[0].reg.value == ZYDIS_REGISTER_RAX
                && ins->operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
            {
                hasMov = true;
            }
            else if (
                ins->mnemonic == ZYDIS_MNEMONIC_XCHG && hasPushRax && hasMov
                && ins->operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY
                && ins->operands[0].mem.base == ZYDIS_REGISTER_RSP
                && !ins->operands[0].mem.disp.hasDisplacement
                && ins->operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER
                && ins->operands[1].reg.value == ZYDIS_REGISTER_RAX)
            {
                hasXchg = true;
            }
            else if (
                ins->mnemonic == ZYDIS_MNEMONIC_JMP && hasPushRax && hasMov
                && hasXchg)
            {
                Logging::Msg("Found call pattern!");

                ins->mnemonic = ZYDIS_MNEMONIC_CALL;

                // Erase the ones behind.
                decoded.erase(
                    decoded.begin() + decoded.size() - 4,
                    decoded.begin() + decoded.size() - 1);

                ins = &decoded.back();
                hasPushRax = false;
                hasMov = false;
                hasXchg = false;
            }
            else
            {
                hasPushRax = false;
                hasMov = false;
                hasXchg = false;
            }
        }

        if (IsBranchTerminal(*ins))
        {
            break;
        }

        va += ins->length;
    }

    return decoded;
}

class AsmJitErrorHandler : public asmjit::ErrorHandler
{
public:
    AsmJitErrorHandler()
        : err(asmjit::kErrorOk)
    {
    }

    void handleError(
        asmjit::Error err,
        const char* message,
        asmjit::BaseEmitter* origin) override
    {
        this->err = err;
        Logging::Msg("asmjit error: %s", message);
    }

    asmjit::Error err;
};

static AsmJitErrorHandler _asmjitErrorHandler;

uintptr_t Rewriter::ProcessBranch(uintptr_t source)
{
    std::lock_guard<std::mutex> lock(_lock);

    // Check if this branch already exists.
    auto it = _sourceToTarget.find(source);
    if (it != _sourceToTarget.end())
        return it->second;

    if constexpr (Logging::LoggingEnabled)
    {
        Logging::Msg("Branch discovery at %p", source);
    }

    DecodedBranch decodedBranch = DecodeBranch(source);
    if constexpr (Logging::LoggingEnabled)
    {
        PrintBranchInstructions(decodedBranch);
    }

    asmjit::CodeHolder code;
    code.init(_jitRT.codeInfo());
    code.setErrorHandler(&_asmjitErrorHandler);

    asmjit::x86::Assembler assembler(&code);

    uintptr_t endVA = 0;

    for (auto& ins : decodedBranch)
    {
        // Redirects control flow to already rewritten branches.
        // May cause issues on x64 with exceptions but improves overall
        // performance.
        if constexpr (false)
        {
            if (IsDirectControlFlow(ins)
                && ins.operands[0].type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
            {
                uintptr_t targetVA = 0;
                if (ZydisCalcAbsoluteAddress(&ins, &ins.operands[0], &targetVA)
                    == ZYDIS_STATUS_SUCCESS)
                {
                    auto itTargetBranch = _sourceToTarget.find(targetVA);
                    if (itTargetBranch != _sourceToTarget.end())
                    {
                        ins.operands[0].imm.isRelative = false;
                        ins.operands[0].imm.isSigned = false;
                        ins.operands[0].imm.value.u = itTargetBranch->second;
                        if constexpr (Logging::LoggingEnabled)
                        {
                            Logging::Msg(
                                "Redirected call to cached branch: %p",
                                itTargetBranch->second);
                        }
                    }
                }
            }
        }

        if (!Translation::convertInstruction(ins, assembler))
        {
            Logging::Msg(
                "Failed to translate instruction: 0x%p", ins.instrAddress);
            return 0;
        }
        endVA = ins.instrAddress + ins.length;
    }

    // Append jump back to end of branch at original VA.
    assembler.jmp(endVA);

    void* fn = nullptr;
    asmjit::Error err = _jitRT.add(&fn, &code, source);
    if (err)
    {
        Logging::Msg("Failed to add function to JIT runtime %08X", err);
        return 0;
    }

    uintptr_t destVA = reinterpret_cast<uintptr_t>(fn);
    _sourceToTarget.emplace(source, destVA);
    _targetToSource.emplace(destVA, source);

    // Validate output.
    if constexpr (true)
    {
        ZydisFormatter fmt;
        ZydisFormatterInit(&fmt, ZYDIS_FORMATTER_STYLE_INTEL);

        DecodedBranch decodedRewrittenBranch = DecodeBranch(destVA, true);

        char bufferLeft[64]{};
        char bufferRight[64]{};

        for (size_t i = 0; i < decodedBranch.size(); i++)
        {
            auto& insLeft = decodedBranch[i];
            auto& insRight = decodedRewrittenBranch[i];

            ZydisFormatterFormatInstruction(
                &fmt, &insLeft, bufferLeft, sizeof(bufferLeft));
            ZydisFormatterFormatInstruction(
                &fmt, &insRight, bufferRight, sizeof(bufferRight));

            if (strcmp(bufferLeft, bufferRight) != 0)
            {
                Logging::Msg(
                    "Rewritten instructions do not match: [%zu] ( %s ) != ( %s "
                    ")",
                    i, bufferLeft, bufferRight);
            }
        }
    }

    if constexpr (Logging::LoggingEnabled)
    {
        Logging::Msg(
            "Branch %p rewritten to %p, len %zu bytes", source, destVA,
            code.codeSize());
    }

    return destVA;
}

} // namespace CovCane
