#include "ExceptionHandler.h"
#include "Memory.h"
#include "Logging.h"
#include "Rewriter.h"

#include <map>
#include <windows.h>
#include <mutex>

namespace CovCane {

static std::vector<std::pair<uintptr_t, uintptr_t>> _sectionMap;

static bool AddressInSectionMap(uintptr_t addr)
{
    for (auto& pair : _sectionMap)
    {
        if (addr >= pair.first && addr < pair.second)
            return true;
    }
    return false;
}

static LONG Handler(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
    if constexpr (false)
    {
        Logging::Msg(
            "Exception: %08X at %p",
            ExceptionInfo->ExceptionRecord->ExceptionCode,
            ExceptionInfo->ExceptionRecord->ExceptionAddress);
    }

    const uintptr_t exceptionAddress = reinterpret_cast<uintptr_t>(
        ExceptionInfo->ExceptionRecord->ExceptionAddress);

    const uint32_t exceptionCode = ExceptionInfo->ExceptionRecord
                                       ->ExceptionCode;
    if (exceptionCode == EXCEPTION_ACCESS_VIOLATION
        && ExceptionInfo->ExceptionRecord->ExceptionInformation[0] == 8
        && AddressInSectionMap(exceptionAddress))
    {
        uintptr_t newIP = Rewriter::ProcessBranch(exceptionAddress);
        if (newIP != 0)
        {
#if _M_X64
            ExceptionInfo->ContextRecord->Rip = newIP;
#else
            ExceptionInfo->ContextRecord->Eip = newIP;
#endif
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    else
    {
        if constexpr (true)
        {
            Logging::Msg(
                "Exception: %08X at %p",
                ExceptionInfo->ExceptionRecord->ExceptionCode,
                ExceptionInfo->ExceptionRecord->ExceptionAddress);
        }
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

static bool RemoveExecutableRights(HMODULE mod)
{
    const uintptr_t imageBase = reinterpret_cast<uintptr_t>(mod);

    IMAGE_DOS_HEADER dosHdr{};
    if (!Memory::SafeRead(imageBase, dosHdr))
    {
        Logging::Msg("Unable to read IMAGE_DOS_HEADER at %p", (void*)imageBase);
        return false;
    }

    IMAGE_NT_HEADERS ntHdr{};
    if (!Memory::SafeRead(imageBase + dosHdr.e_lfanew, ntHdr))
    {
        Logging::Msg(
            "Unable to read IMAGE_NT_HEADERS at %p",
            (void*)(imageBase + dosHdr.e_lfanew));
        return false;
    }

    uintptr_t sectionAddress = imageBase + dosHdr.e_lfanew
                               + sizeof(IMAGE_NT_HEADERS);
    for (int i = 0; i < ntHdr.FileHeader.NumberOfSections; i++)
    {
        IMAGE_SECTION_HEADER sctHdr{};
        if (!Memory::SafeRead(sectionAddress, sctHdr))
        {
            Logging::Msg(
                "Unable to read IMAGE_SECTION_HEADER at %p",
                (void*)sectionAddress);
            return false;
        }

        // Properly null terminate the section name.
        char sectionName[9]{};
        strncpy_s(sectionName, (const char*)sctHdr.Name, 8);

        const uintptr_t sectionVA = imageBase + sctHdr.VirtualAddress;
        const uintptr_t sectionEndVA = sectionVA + sctHdr.Misc.VirtualSize;

        if (sctHdr.Characteristics & IMAGE_SCN_MEM_EXECUTE)
        {
            Logging::Msg(
                "Section: %s, %p - %p", sectionName, (void*)sectionVA,
                (void*)sectionEndVA);

            const uintptr_t sectionLength = sectionEndVA - sectionVA;

            DWORD oldProt;
            if (VirtualProtect(
                    reinterpret_cast<LPVOID>(sectionVA), sectionLength,
                    PAGE_READONLY, &oldProt)
                == FALSE)
            {
                Logging::Msg(
                    "VirtualProtect(%p) failed: 0x%08X", (void*)sectionVA,
                    GetLastError());
            }
            else
            {
                Logging::Msg("Removed execute from %s", sectionName);
                _sectionMap.emplace_back(sectionVA, sectionEndVA);

                Rewriter::CreateSectionBuffer(sectionVA, sectionEndVA);
            }
        }

        sectionAddress += sizeof(IMAGE_SECTION_HEADER);
    }

    return true;
}

bool ExceptionHandler::Initialize()
{
    AddVectoredExceptionHandler(1, Handler);

    // TODO: Allow multiple modules.
    RemoveExecutableRights(GetModuleHandleA(nullptr));

    return true;
}

} // namespace CovCane
