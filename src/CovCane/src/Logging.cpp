#include "Logging.h"
#include <windows.h>

namespace CovCane::Logging {

static FILE* _logFile = nullptr;

namespace Detail {

    void DebugMsg(const char* str)
    {
        size_t bufLen = strlen(str) + 32;
        std::unique_ptr<char[]> prefixed(new char[bufLen]);
        strcpy_s(prefixed.get(), bufLen, "[CovCane] ");
        strcat_s(prefixed.get(), bufLen, str);
        OutputDebugStringA(prefixed.get());
    }

    void Msg(const char* str)
    {
        if (_logFile != nullptr)
        {
            fputs(str, _logFile);
            fputs("\n", _logFile);
            fflush(_logFile);
        }
    }

} // namespace Detail

bool Initialize(const char* outputFile)
{
    fopen_s(&_logFile, outputFile, "wt");
    if (_logFile == nullptr)
        return false;
    return true;
}

void Flush()
{
    if (_logFile)
        fflush(_logFile);
}

} // namespace CovCane::Logging
