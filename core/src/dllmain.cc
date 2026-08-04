#include "pengu.h"
#include "hook.h"
#include "bootstrap.h"
#include "include/cef_version.h"

bool check_libcef_version(bool is_browser);
void HookBrowserProcess();
void HookRendererProcess();

#if OS_WIN

#include <tlhelp32.h>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
void InjectThisDll(HANDLE hProcess);

static bool wcsfindi(const wchar_t *str, const wchar_t *sub)
{
    size_t str_len = wcslen(str), sub_len = wcslen(sub);
    if (sub_len > str_len)
        return false;
    for (size_t i = 0; i <= str_len - sub_len; ++i) {
        for (size_t j = 0; j < sub_len; ++j)
            if (towlower(str[i + j]) != towlower(sub[j]))
                goto next;
        return true;
        next:;
    }
    return false;
}

static hook::Hook<decltype(&CreateProcessW)> Old_CreateProcessW;
static BOOL WINAPI Hooked_CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
    LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
    bool is_renderer = wcsfindi(lpCommandLine, L"LeagueClientUxRender.exe")
        && wcsfindi(lpCommandLine, L"--type=renderer");

    if (is_renderer)
        dwCreationFlags |= CREATE_SUSPENDED;

    BOOL success = Old_CreateProcessW(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
        bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);

    if (success && is_renderer)
    {
        InjectThisDll(lpProcessInformation->hProcess);
        ResumeThread(lpProcessInformation->hThread);
    }

    return success;
}

static void Initialize()
{
    WCHAR exe_path[2048]{};
    GetModuleFileNameW(nullptr, exe_path, _countof(exe_path));

    // Determine which process to be hooked.
    // Browser process.
    if (wcsfindi(exe_path, L"LeagueClientUx.exe"))
    {
        if (check_libcef_version(true))
        {
            HookBrowserProcess();

            // Hook CreateProcessW.
            Old_CreateProcessW.hook(&CreateProcessW, Hooked_CreateProcessW);
        }
    }
    // Render process.
    else if (wcsfindi(exe_path, L"LeagueClientUxRender.exe"))
    { 
        // Renderer only.
        if (wcsstr(GetCommandLineW(), L"--type=renderer") != nullptr)
        {
            if (check_libcef_version(false))
                HookRendererProcess();
        }
    }
}

// DLL entry point.
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(module);
            Initialize();
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }

    return TRUE;
}

static void InjectThisDll(HANDLE hProcess)
{
    WCHAR thisDllPath[2048]{};
    GetModuleFileNameW((HMODULE)&__ImageBase, thisDllPath, _countof(thisDllPath));
    bootstrap::inject(hProcess, thisDllPath);
}

// Ordinal-6000 export, invoked as `rundll32 "core.dll", #6000 <lcux cmdline>`.
//
// Retained for installs whose IFEO value still points here; new activations go
// through boot.dll, which resolves the runtime from the per-user `active`
// pointer instead of being the runtime. Both share bootstrap::launch, so the
// sequence can't drift between them.
int APIENTRY _BootstrapEntry(HWND, HINSTANCE, LPWSTR commandLine, int)
{
    WCHAR thisDllPath[2048]{};
    GetModuleFileNameW((HMODULE)&__ImageBase, thisDllPath, _countof(thisDllPath));

    return bootstrap::launch(commandLine, thisDllPath);
}

#elif OS_MAC

__attribute__((constructor)) static void dllmain(int argc, const char **argv)
{
    std::string prog(argv[0]);
    prog = prog.substr(prog.rfind('/') + 1);

    if (prog == "LeagueClientUx")
    {
#if _DEBUG
        char msg[128];
        snprintf(msg, sizeof(msg)-1, "Debug me: %d", getpid());
        dialog::alert("Continue debugging...", msg);
#endif
        if (check_libcef_version(true))
        {
            HookBrowserProcess();
        }
    }
    else if (prog == "LeagueClientUx Helper (Renderer)")
    {
        if (check_libcef_version(false))
        {
            HookRendererProcess();
        }
    }
}

#endif

int _GetCefVersion()
{
    return CEF_VERSION_MAJOR;
}