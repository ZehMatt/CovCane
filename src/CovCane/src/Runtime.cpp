#include <stdint.h>
#include "Runtime.h"
#include "Logging.h"

namespace CovCane {

static uint8_t* AllocateNearby(uintptr_t va, size_t len, uint32_t pageProtect)
{
    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);

    va = va + (~va & (sysInfo.dwPageSize - 1));

    auto allocateNearbyDelta = [&](intptr_t delta) -> void* {
        MEMORY_BASIC_INFORMATION mbi;

        const intptr_t maxSize = std::numeric_limits<int32_t>::max();
        intptr_t deltaSize = 0;
        for (uintptr_t nearbyVA = va; deltaSize < maxSize;
             nearbyVA += delta, deltaSize += std::abs(delta))
        {
            if (!VirtualQuery(
                    reinterpret_cast<LPCVOID>(nearbyVA), &mbi, sizeof(mbi)))
                break;

            if (mbi.State != MEM_FREE)
                continue;

            uintptr_t baseVA = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            baseVA -= (baseVA & 0xFFFF);

            void* res = VirtualAlloc(
                reinterpret_cast<LPVOID>(baseVA), len, MEM_RESERVE | MEM_COMMIT,
                pageProtect);
            if (res)
                return res;
        }
        return nullptr;
    };

    const intptr_t pageSize = sysInfo.dwPageSize;

    // Up
    void* res = allocateNearbyDelta(+pageSize);
    if (res)
        return static_cast<uint8_t*>(res);

    // Down
    return static_cast<uint8_t*>(allocateNearbyDelta(-pageSize));
}

Runtime::Runtime() noexcept
{
    // Setup target properties.
    _targetType = kTargetJit;
    _codeInfo._archInfo = asmjit::CpuInfo::host().archInfo();
    _codeInfo._stackAlignment = sizeof(uintptr_t);
    _codeInfo._cdeclCallConv = asmjit::CallConv::kIdHostCDecl;
    _codeInfo._stdCallConv = asmjit::CallConv::kIdHostStdCall;
    _codeInfo._fastCallConv = asmjit::CallConv::kIdHostFastCall;
}

void* Runtime::alloc(size_t len, uintptr_t sourceVA)
{
    for (auto& buf : _buffers)
    {
        size_t left = buf.end - buf.cur;
        if (left >= len)
        {
            uint64_t distance = sourceVA > buf.cur ? sourceVA - buf.cur
                                                   : buf.cur - sourceVA;

            // Make sure the distance is not more than 31 bit.
            if (distance >= std::numeric_limits<int32_t>::max())
                continue;

            void* res = reinterpret_cast<void*>(buf.cur);
            buf.cur += len;

            return res;
        }
    }

    // No buffer found, create a new one that is near sourceVA.
    constexpr size_t BufferSize = (1024 * 1024);

    uint8_t* res = AllocateNearby(sourceVA, BufferSize, PAGE_EXECUTE_READWRITE);

    Buffer& buf = _buffers.emplace_back();
    buf.base = reinterpret_cast<uintptr_t>(res);
    buf.cur = buf.base + len;
    buf.end = buf.base + BufferSize;

    return res;
}

void* Runtime::createBuffer(uintptr_t startVA, uintptr_t endVA)
{
    size_t len = endVA - startVA;

    uint8_t* res = AllocateNearby(startVA, len, PAGE_EXECUTE_READWRITE);

    Buffer& buf = _buffers.emplace_back();
    buf.base = reinterpret_cast<uintptr_t>(res);
    buf.cur = buf.base;
    buf.end = buf.base + len;

    Logging::Msg("Created buffer: %p - %p", buf.base, buf.end);

    return res;
}

asmjit::Error Runtime::_add(
    void** dst, asmjit::CodeHolder* code, uintptr_t sourceVA) noexcept
{
    *dst = nullptr;

    ASMJIT_PROPAGATE(code->flatten());
    ASMJIT_PROPAGATE(code->resolveUnresolvedLinks());

    size_t estimatedCodeSize = code->codeSize();
    if (ASMJIT_UNLIKELY(estimatedCodeSize == 0))
        return asmjit::DebugUtils::errored(asmjit::kErrorNoCodeGenerated);

    uint8_t* rw = reinterpret_cast<uint8_t*>(
        alloc(estimatedCodeSize, sourceVA));
    if (rw == nullptr)
    {
        return asmjit::DebugUtils::errored(asmjit::kErrorOutOfMemory);
    }

    // Relocate the code.
    asmjit::Error err = code->relocateToBase(reinterpret_cast<uintptr_t>(rw));
    if (ASMJIT_UNLIKELY(err))
    {
        VirtualFree(rw, estimatedCodeSize, MEM_FREE);
        return err;
    }

    // Recalculate the final code size and shrink the memory we allocated for it
    // in case that some relocations didn't require records in an address table.
    size_t codeSize = code->codeSize();

    for (asmjit::Section* section : code->_sections)
    {
        size_t offset = size_t(section->offset());
        size_t bufferSize = size_t(section->bufferSize());
        size_t virtualSize = size_t(section->virtualSize());

        ASMJIT_ASSERT(offset + bufferSize <= codeSize);
        memcpy(rw + offset, section->data(), bufferSize);

        if (virtualSize > bufferSize)
        {
            ASMJIT_ASSERT(offset + virtualSize <= codeSize);
            memset(rw + offset + bufferSize, 0, virtualSize - bufferSize);
        }
    }

    flush(rw, codeSize);
    *dst = rw;

    return asmjit::kErrorOk;
}

asmjit::Error Runtime::_release(void* p) noexcept
{
    return asmjit::kErrorOk;
}

void Runtime::flush(const void* p, size_t size) noexcept
{
    ::FlushInstructionCache(GetCurrentProcess(), p, size);
}

} // namespace CovCane
