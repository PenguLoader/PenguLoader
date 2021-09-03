#include <windows.h>

static void *(*_Direct3DCreate9)(UINT);
void *Direct3DCreate9(UINT SDKVersion)
{
    return _Direct3DCreate9(SDKVersion);
}

static HRESULT (*_Direct3DCreate9Ex)(UINT, void **);
HRESULT Direct3DCreate9Ex(UINT SDKVersion, void **ppEx)
{
    return _Direct3DCreate9Ex(SDKVersion, ppEx);
}

static int (WINAPI *_D3DPERF_BeginEvent)(DWORD, LPCWSTR);
int WINAPI D3DPERF_BeginEvent(DWORD col, LPCWSTR wszName)
{
    return _D3DPERF_BeginEvent(col, wszName);
}

static int (WINAPI *_D3DPERF_EndEvent)();
int WINAPI D3DPERF_EndEvent()
{
    return _D3DPERF_EndEvent();
}

static DWORD (WINAPI *_D3DPERF_GetStatus)();
DWORD WINAPI D3DPERF_GetStatus()
{
    return _D3DPERF_GetStatus();
}

static void (WINAPI *_D3DPERF_SetMarker)(DWORD, LPCWSTR);
void WINAPI D3DPERF_SetMarker(DWORD col, LPCWSTR wszName)
{
    _D3DPERF_SetMarker(col, wszName);
}

void LoadD3D9APIs()
{
    BOOL wow64 = FALSE;
    CHAR d3d9Path[MAX_PATH];

    if (IsWow64Process(GetCurrentProcess(), &wow64) && wow64) {
        GetSystemWow64DirectoryA(d3d9Path, MAX_PATH);
    }
    else {
        GetSystemDirectoryA(d3d9Path, MAX_PATH);
    }

    lstrcatA(d3d9Path, "\\d3d9.dll");
    HMODULE d3d9 = LoadLibraryA(d3d9Path);

    (LPVOID &)_Direct3DCreate9 = GetProcAddress(d3d9, "Direct3DCreate9");
    (LPVOID &)_Direct3DCreate9Ex = GetProcAddress(d3d9, "Direct3DCreate9Ex");
    (LPVOID &)_D3DPERF_BeginEvent = GetProcAddress(d3d9, "D3DPERF_BeginEvent");
    (LPVOID &)_D3DPERF_EndEvent = GetProcAddress(d3d9, "D3DPERF_EndEvent");
    (LPVOID &)_D3DPERF_GetStatus = GetProcAddress(d3d9, "D3DPERF_GetStatus");
    (LPVOID &)_D3DPERF_SetMarker = GetProcAddress(d3d9, "D3DPERF_SetMarker");
}