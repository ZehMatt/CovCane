#pragma once

#include <stdint.h>

namespace CovCane::Memory {

bool SafeRead(uintptr_t addr, void* dest, size_t len);
template<typename T> bool SafeRead(uintptr_t addr, T& buf)
{
    return SafeRead(addr, &buf, sizeof(T));
}

bool SafeWrite(uintptr_t addr, const void* src, size_t len);
template<typename T> bool SafeWrite(uintptr_t addr, const T& buf)
{
    return SafeWrite(addr, &buf, sizeof(T));
}

} // namespace CovCane::Memory