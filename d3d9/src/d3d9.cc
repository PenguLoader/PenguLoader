#include <windows.h>

static FARPROC GetD3D9Function(const char *name)
{
    static HMODULE module = nullptr;

    if (module == nullptr)
    {
        BOOL wow64 = FALSE;
        WCHAR d3d9Path[MAX_PATH];

        // Get system32 path.
        if (IsWow64Process(GetCurrentProcess(), &wow64) && wow64)
            GetSystemWow64DirectoryW(d3d9Path, MAX_PATH);
        else
            GetSystemDirectoryW(d3d9Path, MAX_PATH);

        lstrcatW(d3d9Path, L"\\d3d9.dll");
        module = LoadLibraryW(d3d9Path);
    }

    return GetProcAddress(module, name);
}

template<typename T>
static T _Forward(T, const char* funcName)
{
    static T proc = nullptr;
    if (proc != nullptr) return proc;
    return proc = reinterpret_cast<T>(GetD3D9Function(funcName));
}

#define Forward(F) _Forward(F, #F)

LPVOID Direct3DCreate9(UINT SDKVersion)
{
    return Forward(Direct3DCreate9)(SDKVersion);
}

HRESULT Direct3DCreate9Ex(UINT SDKVersion, LPVOID ppEx)
{
    return Forward(Direct3DCreate9Ex)(SDKVersion, ppEx);
}

int WINAPI D3DPERF_BeginEvent(DWORD col, LPCWSTR wszName)
{
    return Forward(D3DPERF_BeginEvent)(col, wszName);
}

int WINAPI D3DPERF_EndEvent()
{
    return Forward(D3DPERF_EndEvent)();
}

DWORD WINAPI D3DPERF_GetStatus()
{
    return Forward(D3DPERF_GetStatus)();
}

void WINAPI D3DPERF_SetMarker(DWORD col, LPCWSTR wszName)
{
    return Forward(D3DPERF_SetMarker)(col, wszName);
}