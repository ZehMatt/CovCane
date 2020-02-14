#include <iostream>
#include <string>

#include <windows.h>

static bool InjectLibrary(HANDLE hProc)
{
    // FIXME: This will not work if the child process is not large address
    // aware.
    FARPROC fpLoadLibraryA = GetProcAddress(
        GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

    char dllPath[4048]{};
    GetModuleFileNameA(nullptr, dllPath, sizeof(dllPath));

    char* slash = strrchr(dllPath, '\\');
    if (slash != nullptr)
    {
        *slash = '\0';
    }
    strcat_s(dllPath, "\\CovCane.dll");

    const size_t stringSize = strlen(dllPath);
    const size_t stringBufferSize = stringSize + 256;

    void* dllPathString = VirtualAllocEx(
        hProc, nullptr, stringBufferSize, MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE);

    size_t bytesWritten = 0;
    if (WriteProcessMemory(
            hProc, dllPathString, dllPath, stringSize + 1, &bytesWritten)
        == FALSE)
    {
        printf("WriteProcessMemory failed: 0x%08X\n", GetLastError());
        return false;
    }

    DWORD threadId = 0;
    HANDLE hThread = CreateRemoteThread(
        hProc, nullptr, 0, (LPTHREAD_START_ROUTINE)fpLoadLibraryA,
        dllPathString, 0, &threadId);

    if (hThread == nullptr)
    {
        printf(
            "Failed to inject dll, CreateRemoteThread failed: 0x%08X\n",
            GetLastError());
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    printf("Successfully injected dll: %s\n", dllPath);

    return true;
}

int main(int argc, const char* argv[])
{
    if (argc <= 1)
    {
        printf("Missing argument: <process>\n");
        return EXIT_FAILURE;
    }

    PROCESS_INFORMATION pi{};
    STARTUPINFOA si{};
    si.cb = sizeof(si);

    char commandLine[1024]{};
    sprintf_s(commandLine, "%s", argv[1]);

    // Start the child process.
    if (CreateProcessA(
            nullptr, commandLine, nullptr, nullptr, FALSE, CREATE_SUSPENDED,
            nullptr, nullptr, &si, &pi)
        == FALSE)
    {
        printf("CreateProcess failed (%d).\n", GetLastError());
        return EXIT_FAILURE;
    }

    if (!InjectLibrary(pi.hProcess))
    {
        printf("Injection failed!.\n");

        TerminateProcess(pi.hProcess, EXIT_FAILURE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return EXIT_FAILURE;
    }

    // Resume thread.
    if (ResumeThread(pi.hThread) == FALSE)
    {
        printf(
            "Unable to resume execution, ResumeThread: 0x%08X\n",
            GetLastError());

        TerminateProcess(pi.hProcess, EXIT_FAILURE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        return EXIT_FAILURE;
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode;
}