#pragma once

#include <memory>
#include <cstdio>

namespace CovCane::Logging {

namespace Detail {
    void Msg(const char* str);
    void DebugMsg(const char* str);
} // namespace Detail

constexpr bool LoggingEnabled = true;

bool Initialize(const char* outputFile);

template<typename... Args> void DebugMsg(const char* fmt, Args&&... args)
{
    char buffer[128]{};
    int res = snprintf(
        buffer, sizeof(buffer), fmt, std::forward<Args&&>(args)...);
    if (res < sizeof(buffer))
    {
        Detail::DebugMsg(buffer);
    }
    else
    {
        // Requires more room.
        std::unique_ptr<char[]> buf(new char[res + 1]);
        res = snprintf(buf.get(), res + 1, fmt, std::forward<Args&&>(args)...);
        Detail::DebugMsg(buf.get());
    }
}

template<typename... Args> void Msg(const char* fmt, Args&&... args)
{
    char buffer[128]{};
    int res = snprintf(
        buffer, sizeof(buffer), fmt, std::forward<Args&&>(args)...);
    if (res < sizeof(buffer))
    {
        Detail::Msg(buffer);
    }
    else
    {
        // Requires more room.
        std::unique_ptr<char[]> buf(new char[res + 1]);
        res = snprintf(buf.get(), res + 1, fmt, std::forward<Args&&>(args)...);
        Detail::Msg(buf.get());
    }
}

void Flush();

} // namespace CovCane::Logging