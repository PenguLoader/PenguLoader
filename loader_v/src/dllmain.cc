#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <initializer_list>
#include <string>
#include <vector>
#include <thread>
#include <detours/detours.h>

#define NETHOST_USE_AS_STATIC
#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <nethost.h>

/// This DLL should be loaded by rundll32.exe
/// to load .NET runtime and launch loader.dll for debugging purposes.

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#define THIS_MODULE ((HMODULE)&__ImageBase)

#undef MAX_PATH
#define MAX_PATH 2048

static bool _wcsi_endw(LPCWSTR str, LPCWSTR suffix)
{
	size_t str_len = wcslen(str);
	size_t suffix_len = wcslen(suffix);
	if (str_len < suffix_len)
		return false;
	return _wcsicmp(str + (str_len - suffix_len), suffix) == 0;
}

static hostfxr_initialize_for_dotnet_command_line_fn hostfxr_init_for_cmd_line_fptr;
static hostfxr_get_runtime_delegate_fn hostfxr_get_delegate_fptr;
static hostfxr_run_app_fn hostfxr_run_app_fptr;
static hostfxr_close_fn hostfxr_close_fptr;
static hostfxr_set_error_writer_fn hostfxr_set_error_writer;
static hostfxr_initialize_for_runtime_config_fn hostfxr_initialize_for_runtime_config;

static bool LoadHostfxr()
{
	WCHAR dll_path[MAX_PATH]{};
	size_t path_size = MAX_PATH;

	if (get_hostfxr_path(dll_path, &path_size, nullptr) != 0)
		return false;

	HMODULE lib = LoadLibrary(dll_path);
	if (lib == nullptr)
		return false;

	(LPVOID &)hostfxr_init_for_cmd_line_fptr = GetProcAddress(lib, "hostfxr_initialize_for_dotnet_command_line");
	(LPVOID &)hostfxr_get_delegate_fptr = GetProcAddress(lib, "hostfxr_get_runtime_delegate");
	(LPVOID &)hostfxr_run_app_fptr = GetProcAddress(lib, "hostfxr_run_app");
	(LPVOID &)hostfxr_close_fptr = GetProcAddress(lib, "hostfxr_close");
	(LPVOID &)hostfxr_set_error_writer = GetProcAddress(lib, "hostfxr_set_error_writer");
}

static std::wstring GetThisPath(bool dir)
{
	WCHAR pathbuf[MAX_PATH]{};
	size_t length = GetModuleFileNameW(THIS_MODULE, pathbuf, MAX_PATH);

	std::wstring dllPath{ pathbuf, length };
	if (!dir)
		return dllPath;

	return dllPath.substr(0, dllPath.find_last_of(L"/\\"));
}

static bool FileExists(const std::wstring &path)
{
	DWORD attrs = GetFileAttributesW(path.c_str());
	return (attrs != INVALID_FILE_ATTRIBUTES)
		&& !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static bool IsLoaderDll(std::wstring &out, const std::wstring &path, bool from_dir)
{
    std::wstring path2 = from_dir ? path
		: path.substr(0, path.find_last_of(L"/\\"));

    path2 += L"\\loader.dll";

	if (FileExists(path2))
	{
		out = path2;
		return true;
    }

    return false;
}

static bool FindLoaderDll(std::wstring &out_path)
{
	out_path.clear();
	auto this_dll = GetThisPath(false);

    // first check the same dir (IFEO mode)
	if (IsLoaderDll(out_path, this_dll, false))
        return true;

	HANDLE h = CreateFile(
		this_dll.c_str(), GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

    // check if symlink, then get the final path
	if ((GetFileAttributes(this_dll.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) == FILE_ATTRIBUTE_REPARSE_POINT)
	{
        WCHAR _dest[MAX_PATH];
		DWORD len = GetFinalPathNameByHandleW(h, _dest, MAX_PATH, FILE_NAME_OPENED);

        std::wstring dest{ _dest, len };

		// remove prepended '\\?\'
		if (dest.find(L"\\\\?\\") == 0)
			dest.erase(0, 4);

		CloseHandle(h);
        return IsLoaderDll(out_path, dest, false);
    }

	// otherwise this dll is hard linked
	// enumerate all links to find the dir containing loader.dll

	DWORD size = MAX_PATH;
	WCHAR buffer[MAX_PATH];

	HANDLE hEnum = FindFirstFileNameW(
		this_dll.c_str(),
		0,
		&size,
		buffer);

	if (hEnum == INVALID_HANDLE_VALUE)
		return false;

	auto drive_name = this_dll.substr(0, this_dll.find(L":\\") + 1);

	do
	{
		std::wstring path{ buffer, size };
		size = MAX_PATH;

		if (IsLoaderDll(out_path, drive_name + path, false))
			break;

	} while (FindNextFileNameW(hEnum, &size, buffer));

	FindClose(hEnum);
	CloseHandle(h);

	return !out_path.empty();
}

static bool LoadLoaderEntry(const std::initializer_list<LPCWSTR> *pArgs)
{
	if (!LoadHostfxr()) {
		MessageBox(0, L"Failed to load hostfxr", L"[loader_v] Error", MB_OK | MB_ICONWARNING);
		return false;
	}

	hostfxr_set_error_writer([](const char_t *err)
		{
			MessageBox(0, err, L"[loader_v] Hostfxr Error", MB_OK | MB_ICONWARNING);
		});

	std::wstring loader_dll;
	if (!FindLoaderDll(loader_dll)) {
		MessageBox(0, L"Failed to find loader.dll", L"[loader_v] Error", MB_OK | MB_ICONWARNING);
		return false;
	}

	std::vector<LPCWSTR> args{};
	args.insert(args.begin(), loader_dll.c_str());

	if (pArgs != nullptr)
		args.insert(args.end(), *pArgs);
	else
		args.push_back(L"-loader_v");

	hostfxr_handle ctx = nullptr;
	hostfxr_init_for_cmd_line_fptr((int)args.size(), args.data(), nullptr, &ctx);

	std::thread([ctx]()
		{
			hostfxr_run_app_fptr(ctx);
			hostfxr_close_fptr(ctx);
		}).detach();

	return true;
}

static void InjectThisDll(HANDLE target)
{
	auto this_dll = GetThisPath(false);
	size_t path_size = (this_dll.length() + 1) * sizeof(WCHAR);

	LPVOID path_addr = VirtualAllocEx(target, NULL, path_size, MEM_COMMIT, PAGE_READWRITE);
	if (!path_addr)
	{
		MessageBox(NULL,
			__FUNCTIONW__ L": Failed to allocate memory in target process.",
			L"[loader_v] Error", MB_OK | MB_ICONWARNING);
		return;
	}

	BOOL success = WriteProcessMemory(target, path_addr, this_dll.c_str(), path_size, NULL);
	if (!success)
	{
		MessageBox(NULL,
			__FUNCTIONW__ L": Failed to write process memory.",
			L"[loader_v] Error", MB_OK | MB_ICONWARNING);
		return;
	}

	HANDLE thread = CreateRemoteThread(target, NULL, 0, (LPTHREAD_START_ROUTINE)&LoadLibraryW, path_addr, 0, NULL);
	if (!thread)
	{
		MessageBox(NULL,
			__FUNCTIONW__ L": Failed to create remote thread.",
			L"[loader_v] Error", MB_OK | MB_ICONWARNING);
		return;
	}

	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);

	VirtualFreeEx(target, path_addr, 0, MEM_RELEASE);
	CreateRemoteThread(target, NULL, 0, (LPTHREAD_START_ROUTINE)&LoadLoaderEntry, NULL, 0, NULL);
}

// Entry point for rundll32.exe
EXTERN_C __declspec(dllexport)
int APIENTRY LaunchLoaderAppW(HWND hwnd, HINSTANCE hinst, LPWSTR cmdline, int)
{
	LONG(NTAPI * NtQueryInformationProcess)(HANDLE, DWORD, PVOID, ULONG, PULONG);
	LONG(NTAPI * NtRemoveProcessDebug)(HANDLE, HANDLE);
	LONG(NTAPI * NtClose)(HANDLE Handle);

	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE,
		CREATE_SUSPENDED | DEBUG_ONLY_THIS_PROCESS, NULL, NULL, &si, &pi))
	{
		char msg[128];
		sprintf_s(msg, "Failed to create LeagueClientUx process, last error: 0x%08X.", GetLastError());
		MessageBoxA(NULL, msg, "Pengu Loader bootstrapper", MB_ICONWARNING | MB_OK | MB_TOPMOST);
		return 1;
	}

	HMODULE ntdll = GetModuleHandleA("ntdll");
	(LPVOID &)NtQueryInformationProcess = GetProcAddress(ntdll, "NtQueryInformationProcess");
	(LPVOID &)NtRemoveProcessDebug = GetProcAddress(ntdll, "NtRemoveProcessDebug");
	(LPVOID &)NtClose = GetProcAddress(ntdll, "NtClose");

	HANDLE hDebug;
	if (NtQueryInformationProcess(pi.hProcess, 30, &hDebug, sizeof(HANDLE), 0) >= 0)
	{
		NtRemoveProcessDebug(pi.hProcess, hDebug);
		NtClose(hDebug);
	}

	InjectThisDll(pi.hProcess);
	ResumeThread(pi.hThread);
	WaitForSingleObject(pi.hProcess, INFINITE);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return 0;
}

static std::wstring g_extraCmdLine;
static auto Old_GetCommandLineW = &GetCommandLineW;
static LPWSTR WINAPI Hooked_GetCommandLineW()
{
	static WCHAR modified[32768]{};
	if (!modified[0])
	{
		wcscpy_s(modified, Old_GetCommandLineW());
		wcscat_s(modified, g_extraCmdLine.c_str());
	}

	return modified;
}

static void Initialize()
{
#if _DEBUG
	AllocConsole();
	freopen_s((FILE **)stdout, "CONOUT$", "w", stdout);
#endif
	
	extern int PickFreeTcpPort();
	int port = 8889;// PickFreeTcpPort();

    printf("[LOADER_V] Selected free TCP port: %d\n", port);

    // append cmdline
	g_extraCmdLine = L" --remote-debugging-port=";
    g_extraCmdLine += std::to_wstring(port);

    // hook GetCommandLineW
	DetourTransactionBegin();
	DetourAttach(
		(LPVOID *)&Old_GetCommandLineW,
		(LPVOID)Hooked_GetCommandLineW);
	DetourTransactionCommit();

	//LoadLoaderEntry(nullptr);
}

BOOL APIENTRY DllMain(HMODULE hinst, DWORD reason, LPVOID)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
	{
		static bool initialized = false;
		DisableThreadLibraryCalls(hinst);

		if (initialized)
			break;

		WCHAR exe_path[MAX_PATH]{};
		GetModuleFileNameW(NULL, exe_path, MAX_PATH);

		// check if 'Riot Client.exe'
		if (_wcsi_endw(exe_path, L"Riot Client.exe"))
		{
			LPCWSTR cmd_line = GetCommandLineW();

			// check if browser process
			if (wcsstr(cmd_line, L"--app-port=") &&
				wcsstr(cmd_line, L"--remoting-auth-token="))
			{
				Initialize();
				initialized = true;
			}
		}

		break;
	}

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}

// reuse core dll proxying impl
#include "../../core/src/dllproxy.cc"