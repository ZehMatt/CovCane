#include <windows.h>
#include "Logging.h"
#include "ExceptionHandler.h"

using namespace CovCane;

static void Startup()
{
    Logging::Initialize("CovCane.log");

    Logging::Msg("Process Id: %u", GetCurrentProcessId());
    Logging::Msg("Image Base: %p", (void*)GetModuleHandleA(nullptr));

    if (!ExceptionHandler::Initialize())
        Logging::Msg("Failed to initialize exception handling.");
    else
        Logging::Msg("Initialized exception handling.");

    if constexpr (false)
    {
        while (!IsDebuggerPresent())
        {
            Sleep(1000);
        }
    }

    Logging::Msg("Environment setup");
}

static void Shutdown()
{
    Logging::Msg("Shutdown");
    Logging::Flush();
}

BOOL APIENTRY
    DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    using namespace CovCane;

    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            Startup();
            break;
        case DLL_THREAD_ATTACH:
            break;
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
            Shutdown();
            break;
    }
    return TRUE;
}
