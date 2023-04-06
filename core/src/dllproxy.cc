#include <windows.h>

enum Module
{
    D3D9_DLL,
    DWRITE_DLL,
    VERSION_DLL,

    MODULE_MAX
};

static FARPROC GetFunction(Module index, const char *name)
{
    static HMODULE modules[MODULE_MAX]{};
    static LPCWSTR module_dlls[MODULE_MAX]
    {
        L"d3d9.dll",
        L"dwrite.dll",
        L"version.dll",
    };

    if (modules[index] == nullptr)
    {
        BOOL wow64 = FALSE;
        WCHAR path[MAX_PATH];

        if (IsWow64Process(GetCurrentProcess(), &wow64) && wow64)
            GetSystemWow64DirectoryW(path, MAX_PATH);
        else
            GetSystemDirectoryW(path, MAX_PATH);

        lstrcatW(path, L"\\");
        lstrcatW(path, module_dlls[index]);
        modules[index] = LoadLibraryW(path);
    }

    return GetProcAddress(modules[index], name);
}

template<typename T>
static T _Forward(Module index, const char* funcName, T)
{
    static T proc = nullptr;
    if (proc != nullptr) return proc;
    return proc = reinterpret_cast<T>(GetFunction(index, funcName));
}

// D3D9_DLL
// ==============================
#define Forward_D3D9(F) _Forward(D3D9_DLL, #F, F)

EXTERN_C LPVOID WINAPI Direct3DCreate9(UINT SDKVersion)
{
    return Forward_D3D9(Direct3DCreate9)(SDKVersion);
}

EXTERN_C HRESULT WINAPI Direct3DCreate9Ex(UINT SDKVersion, LPVOID ppEx)
{
    return Forward_D3D9(Direct3DCreate9Ex)(SDKVersion, ppEx);
}

EXTERN_C int WINAPI D3DPERF_BeginEvent(DWORD col, LPCWSTR wszName)
{
    return Forward_D3D9(D3DPERF_BeginEvent)(col, wszName);
}

EXTERN_C int WINAPI D3DPERF_EndEvent()
{
    return Forward_D3D9(D3DPERF_EndEvent)();
}

EXTERN_C DWORD WINAPI D3DPERF_GetStatus()
{
    return Forward_D3D9(D3DPERF_GetStatus)();
}

EXTERN_C BOOL WINAPI D3DPERF_QueryRepeatFrame()
{
    return Forward_D3D9(D3DPERF_QueryRepeatFrame)();
}

EXTERN_C void WINAPI D3DPERF_SetMarker(DWORD col, LPCWSTR wszName)
{
    Forward_D3D9(D3DPERF_SetMarker)(col, wszName);
}

EXTERN_C int WINAPI D3DPERF_SetOptions(DWORD dwOptions)
{
    return Forward_D3D9(D3DPERF_SetOptions)(dwOptions);
}

EXTERN_C void WINAPI D3DPERF_SetRegion(DWORD col, LPCWSTR wszName)
{
    Forward_D3D9(D3DPERF_SetRegion)(col, wszName);
}

// DWRITE_DLL
// ==============================
#define Forward_DWRITE(F) _Forward(DWRITE_DLL, #F, F)

EXTERN_C HRESULT WINAPI DWriteCreateFactory(int factoryType, REFIID iid, IUnknown **factory)
{
    return Forward_DWRITE(DWriteCreateFactory)(factoryType, iid, factory);
}

// VERSION DLL
// ==============================
#define Forward_VERSION(F) _Forward(VERSION_DLL, #F, F)

EXTERN_C BOOL WINAPI GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    return Forward_VERSION(GetFileVersionInfoA)(lptstrFilename, dwHandle, dwLen, lpData);
}

EXTERN_C int WINAPI GetFileVersionInfoByHandle(int hMem, LPCWSTR lpFileName, int v2, int v3)
{
    return Forward_VERSION(GetFileVersionInfoByHandle)(hMem, lpFileName, v2, v3);
}

EXTERN_C BOOL WINAPI GetFileVersionInfoExA(DWORD dwFlags, LPCSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    return Forward_VERSION(GetFileVersionInfoExA)(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData);
}

EXTERN_C BOOL WINAPI GetFileVersionInfoExW(DWORD dwFlags, LPCWSTR lpwstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    return Forward_VERSION(GetFileVersionInfoExW)(dwFlags, lpwstrFilename, dwHandle, dwLen, lpData);
}

EXTERN_C DWORD WINAPI GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle)
{
    return Forward_VERSION(GetFileVersionInfoSizeA)(lptstrFilename, lpdwHandle);
}

EXTERN_C DWORD WINAPI GetFileVersionInfoSizeExA(DWORD dwFlags, LPCSTR lpwstrFilename, LPDWORD lpdwHandle)
{
    return Forward_VERSION(GetFileVersionInfoSizeExA)(dwFlags, lpwstrFilename, lpdwHandle);
}

EXTERN_C DWORD WINAPI GetFileVersionInfoSizeExW(DWORD dwFlags, LPCWSTR lpwstrFilename, LPDWORD lpdwHandle)
{
    return Forward_VERSION(GetFileVersionInfoSizeExW)(dwFlags, lpwstrFilename, lpdwHandle);
}

EXTERN_C DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle)
{
    return Forward_VERSION(GetFileVersionInfoSizeW)(lptstrFilename, lpdwHandle);
}

EXTERN_C BOOL WINAPI GetFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData)
{
    return Forward_VERSION(GetFileVersionInfoW)(lptstrFilename, dwHandle, dwLen, lpData);
}

EXTERN_C DWORD WINAPI VerFindFileA(DWORD uFlags, LPCSTR szFileName, LPCSTR szWinDir, LPCSTR szAppDir, LPSTR szCurDir, PUINT lpuCurDirLen, LPSTR szDestDir, PUINT lpuDestDirLen)
{
    return Forward_VERSION(VerFindFileA)(uFlags, szFileName, szWinDir, szAppDir, szCurDir, lpuCurDirLen, szDestDir, lpuDestDirLen);
}

EXTERN_C DWORD WINAPI VerFindFileW(DWORD uFlags, LPCWSTR szFileName, LPCWSTR szWinDir, LPCWSTR szAppDir, LPWSTR szCurDir, PUINT lpuCurDirLen, LPWSTR szDestDir, PUINT lpuDestDirLen)
{
    return Forward_VERSION(VerFindFileW)(uFlags, szFileName, szWinDir, szAppDir, szCurDir, lpuCurDirLen, szDestDir, lpuDestDirLen);
}

EXTERN_C DWORD WINAPI VerInstallFileA(DWORD uFlags, LPCSTR szSrcFileName, LPCSTR szDestFileName, LPCSTR szSrcDir, LPCSTR szDestDir, LPCSTR szCurDir, LPSTR szTmpFile, PUINT lpuTmpFileLen)
{
    return Forward_VERSION(VerInstallFileA)(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, lpuTmpFileLen);
}

EXTERN_C DWORD WINAPI VerInstallFileW(DWORD uFlags, LPCWSTR szSrcFileName, LPCWSTR szDestFileName, LPCWSTR szSrcDir, LPCWSTR szDestDir, LPCWSTR szCurDir, LPWSTR szTmpFile, PUINT lpuTmpFileLen)
{
    return Forward_VERSION(VerInstallFileW)(uFlags, szSrcFileName, szDestFileName, szSrcDir, szDestDir, szCurDir, szTmpFile, lpuTmpFileLen);
}

EXTERN_C DWORD WINAPI VerLanguageNameA(DWORD wLang, LPSTR szLang, DWORD cchLang)
{
    return Forward_VERSION(VerLanguageNameA)(wLang, szLang, cchLang);
}

EXTERN_C DWORD WINAPI VerLanguageNameW(DWORD wLang, LPWSTR szLang, DWORD cchLang)
{
    return Forward_VERSION(VerLanguageNameW)(wLang, szLang, cchLang);
}

EXTERN_C BOOL WINAPI VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID * lplpBuffer, PUINT puLen)
{
    return Forward_VERSION(VerQueryValueA)(pBlock, lpSubBlock, lplpBuffer, puLen);
}

EXTERN_C BOOL WINAPI VerQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID * lplpBuffer, PUINT puLen)
{
    return Forward_VERSION(VerQueryValueW)(pBlock, lpSubBlock, lplpBuffer, puLen);
}