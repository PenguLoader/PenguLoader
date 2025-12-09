#include "common.h"
#include "../../core/src/hook.h"

#define ProcessDebugObjectHandle 30
#define NT_SUCCESS(status) ((LONG)(status) >= 0)
#define K_BY_RUNDLL32 "BY_RUNDLL32"


/// Reg install:
///
/// HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\Riot Client.exe
/// + Debugger = rundll32.exe %path_to%\loader_v.dll,Rundll32Entry

/// Process tree:
/// 
/// # rundll32.exe (by IFEO debugger)
/// @ CreateProcessW(DEBUG_ONLY_THIS_PROCESS) + RemoveDebugger()
/// 
///   # Riot Client.exe | Browser process
///   @ DllMain() -> LoadLoaderEntry()
///   @ InjectThisDll() -> PreventRecursiveDebugging()
///  
///    # Riot Client.exe | Renderer processes (no IFEO)
///    ...


// Remove debugger from process
static void RemoveDebugger(HANDLE hProcess)
{
    static LONG(NTAPI *NtRemoveProcessDebug)(HANDLE proc, HANDLE dbg);
    static LONG(NTAPI *NtQueryInformationProcess)(HANDLE proc, DWORD pic, PVOID ppi, ULONG szpi, PULONG pret);
    static LONG(NTAPI *NtClose)(HANDLE handle);

    // Load ntdll functions
    if (!NtRemoveProcessDebug)
    {
        HMODULE ntdll = GetModuleHandleA("ntdll");
        (LPVOID &)NtRemoveProcessDebug = GetProcAddress(ntdll, "NtRemoveProcessDebug");
        (LPVOID &)NtQueryInformationProcess = GetProcAddress(ntdll, "NtQueryInformationProcess");
        (LPVOID &)NtClose = GetProcAddress(ntdll, "NtClose");
    }

    HANDLE hDebug;
    if (NT_SUCCESS(NtQueryInformationProcess(hProcess,
        ProcessDebugObjectHandle, &hDebug, sizeof(HANDLE), NULL)))
    {
        NtRemoveProcessDebug(hProcess, hDebug);
        NtClose(hDebug);
    }
}

// Inject this DLL into target process
static void InjectThisDll(HANDLE hProcess)
{
    HMODULE kernel32 = GetModuleHandleA("kernel32");
    auto pVirtualAllocEx = (decltype(&VirtualAllocEx))GetProcAddress(kernel32, "VirtualAllocEx");
    auto pWriteProcessMemory = (decltype(&WriteProcessMemory))GetProcAddress(kernel32, "WriteProcessMemory");
    auto pVirtualFreeEx = (decltype(&VirtualFreeEx))GetProcAddress(kernel32, "VirtualFreeEx");
    auto pCreateRemoteThread = (decltype(&CreateRemoteThread))GetProcAddress(kernel32, "CreateRemoteThread");

    WCHAR thisDllPath[MAX_PATH];
    GetModuleFileNameW(THIS_MODULE, thisDllPath, MAX_PATH);

    size_t pathSize = (wcslen(thisDllPath) + 1) * sizeof(WCHAR);
    LPVOID pathAddr = pVirtualAllocEx(hProcess, NULL, pathSize, MEM_COMMIT, PAGE_READWRITE);
    pWriteProcessMemory(hProcess, pathAddr, thisDllPath, pathSize, NULL);

    HANDLE loader = pCreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)&LoadLibraryW, pathAddr, 0, NULL);
    WaitForSingleObject(loader, INFINITE);
    CloseHandle(loader);

    //pVirtualFreeEx(hProcess, pathAddr, 0, MEM_RELEASE);
}

// Entry point for rundll32.exe
EXTERN_C __declspec(dllexport)
int APIENTRY Rundll32EntryW(HWND hwnd, HINSTANCE hinst, LPWSTR cmdline, int)
{
    // Mark the process as run by rundll32
    SetEnvironmentVariableA(K_BY_RUNDLL32, "1");

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    HMODULE kernel32 = GetModuleHandleA("kernel32");
    auto pCreateProcessW = (decltype(&CreateProcessW))GetProcAddress(kernel32, "CreateProcessW");

    // Create process in suspended state with debugger flag
    if (!pCreateProcessW(NULL, cmdline, NULL, NULL, FALSE,
        CREATE_SUSPENDED | DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si, &pi))
    {
        ShowWraning("Failed to create 'Riot Client.exe' process, last error: 0x%08X.", GetLastError());
        return 1;
    }

    // Remove debugger from created process
    RemoveDebugger(pi.hProcess);

    // Inject itself to do next steps
    InjectThisDll(pi.hProcess);
    ResumeThread(pi.hThread);

    // Wait for process exit
    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}

static hook::Hook<decltype(&CreateProcessW)> Old_CreateProcessW;
static BOOL WINAPI New_CreateProcessW(
    _In_opt_ LPCWSTR lpApplicationName,
    _Inout_opt_ LPWSTR lpCommandLine,
    _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ BOOL bInheritHandles,
    _In_ DWORD dwCreationFlags,
    _In_opt_ LPVOID lpEnvironment,
    _In_opt_ LPCWSTR lpCurrentDirectory,
    _In_ LPSTARTUPINFOW lpStartupInfo,
    _Out_ LPPROCESS_INFORMATION lpProcessInformation)
{
    BOOL success = Old_CreateProcessW(
        lpApplicationName,
        lpCommandLine,
        lpProcessAttributes,
        lpThreadAttributes,
        bInheritHandles,
        dwCreationFlags | DEBUG_ONLY_THIS_PROCESS,
        lpEnvironment,
        lpCurrentDirectory,
        lpStartupInfo,
        lpProcessInformation);

    if (success)
        RemoveDebugger(lpProcessInformation->hProcess);

    return success;
}

static hook::Hook<decltype(&CreateProcessAsUserW)> Old_CreateProcessAsUserW;
static BOOL WINAPI New_CreateProcessAsUserW(
    _In_opt_ HANDLE hToken,
    _In_opt_ LPCWSTR lpApplicationName,
    _Inout_opt_ LPWSTR lpCommandLine,
    _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ BOOL bInheritHandles,
    _In_ DWORD dwCreationFlags,
    _In_opt_ LPVOID lpEnvironment,
    _In_opt_ LPCWSTR lpCurrentDirectory,
    _In_ LPSTARTUPINFOW lpStartupInfo,
    _Out_ LPPROCESS_INFORMATION lpProcessInformation)
{
    BOOL success = Old_CreateProcessAsUserW(
        hToken,
        lpApplicationName,
        lpCommandLine,
        lpProcessAttributes,
        lpThreadAttributes,
        bInheritHandles,
        dwCreationFlags | DEBUG_ONLY_THIS_PROCESS,
        lpEnvironment,
        lpCurrentDirectory,
        lpStartupInfo,
        lpProcessInformation);

    if (success)
        RemoveDebugger(lpProcessInformation->hProcess);

    return success;
}

static void PreventRecursiveDebugging()
{
    // Only hook if we are run by rundll32.exe
    if (GetEnvironmentVariableA(K_BY_RUNDLL32, NULL, 0) > 0)
    {
        LoadLibraryA("advapi32");

        // Hook process creation functions to add debugger flag and
        // remove debugger from created processes to
        // prevent infinite debugging loop by IFEO
        Old_CreateProcessW.hook("kernel32", "CreateProcessW", New_CreateProcessW);
        Old_CreateProcessAsUserW.hook("advapi32", "CreateProcessAsUserW", New_CreateProcessAsUserW);
    }
}

// Auto-run on DLL load, no DllMain needed
struct AutoPreventRecursiveDebugging
{
    AutoPreventRecursiveDebugging()
    {
        PreventRecursiveDebugging();
    }
} _autoPreventRecursiveDebugging;