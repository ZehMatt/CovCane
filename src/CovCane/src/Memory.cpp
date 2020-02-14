#include "Memory.h"
#include <windows.h>

namespace CovCane {

bool Memory::SafeRead(uintptr_t addr, void* dest, size_t len)
{
    size_t bytesRead = 0;
    if (ReadProcessMemory(
            GetCurrentProcess(), reinterpret_cast<LPCVOID>(addr), dest, len,
            &bytesRead)
        == FALSE)
    {
        return false;
    }
    return bytesRead == len;
}

bool Memory::SafeWrite(uintptr_t addr, const void* src, size_t len)
{
    size_t bytesWritten = 0;
    if (WriteProcessMemory(
            GetCurrentProcess(), reinterpret_cast<LPVOID>(addr), src, len,
            &bytesWritten)
        == FALSE)
    {
        return false;
    }
    return bytesWritten == len;
}

} // namespace CovCane
