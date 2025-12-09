#include "common.h"
#include "../../core/src/hook.h"
#include <vector>
#include <thread>

#ifdef _DEBUG
#define NETHOST_USE_AS_STATIC
#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <nethost.h>

static hostfxr_initialize_for_dotnet_command_line_fn hostfxr_init_fptr;
static hostfxr_run_app_fn hostfxr_run_app_fptr;
static hostfxr_close_fn hostfxr_close_fptr;
static hostfxr_set_error_writer_fn hostfxr_set_error_writer;

static bool LoadHostfxr(const char_t *assembly_path)
{
    get_hostfxr_parameters params{};
    params.size = sizeof(get_hostfxr_parameters);
    params.assembly_path = assembly_path;

	WCHAR dll_path[MAX_PATH]{};
	size_t path_size = MAX_PATH;

	if (get_hostfxr_path(dll_path, &path_size, nullptr) != 0)
		return false;

    DebugLog("Found hostfxr path: %ls", dll_path);

	HMODULE lib = LoadLibrary(dll_path);
	if (lib == nullptr)
		return false;

	(LPVOID &)hostfxr_init_fptr = GetProcAddress(lib, "hostfxr_initialize_for_dotnet_command_line");
	(LPVOID &)hostfxr_run_app_fptr = GetProcAddress(lib, "hostfxr_run_app");
	(LPVOID &)hostfxr_close_fptr = GetProcAddress(lib, "hostfxr_close");
	(LPVOID &)hostfxr_set_error_writer = GetProcAddress(lib, "hostfxr_set_error_writer");

	return hostfxr_init_fptr && hostfxr_run_app_fptr;
}
#endif

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

#if _DEBUG
static void LoadLoaderAssembly(const std::wstring &path)
{
	if (!LoadHostfxr(path.c_str())) {
		ShowWraning("Failed to load hostfxr");
		return;
	}

	hostfxr_set_error_writer([](const char_t *err)
		{
			ShowWraning("Hostfxr error: %ws", err);
		});

	std::vector<LPCWSTR> args{};
	args.push_back(path.c_str());
    args.push_back(L"-loader_v");

	hostfxr_handle ctx = nullptr;
	hostfxr_init_fptr((int)args.size(), args.data(), nullptr, &ctx);

	std::thread([ctx]
		{
			hostfxr_run_app_fptr(ctx);
			hostfxr_close_fptr(ctx);
		}
	).detach();
}
#endif

static void Initialize()
{
#if _DEBUG
	AllocConsole();
	freopen_s((FILE **)stdout, "CONOUT$", "w", stdout);
#endif
	
#if _DEBUG
	int rdp_port = 8889;
    DebugLog("Remote debugging port: %d", rdp_port);

	static std::wstring s_cmd_line{};
	s_cmd_line.append(GetCommandLineW());

	if (/*confg::riot_potato_mode()*/ false)
	{
		s_cmd_line += L" --disable-smooth-scrolling --force-prefers-reduced-motion";
		s_cmd_line += L" --wm-window-animations-disabled --animation-duration-scale=0";
	}

	s_cmd_line += L" --remote-debugging-port=";
	s_cmd_line += std::to_wstring(rdp_port);

    // hook GetCommandLineW
	static hook::Hook<decltype(&GetCommandLineW)> Old_GetCommandLineW;
	Old_GetCommandLineW.hook(&GetCommandLineW, []() -> LPWSTR { return s_cmd_line.data(); });
#endif

	std::wstring loader_dll;
	if (!FindLoaderDll(loader_dll)) {
		ShowWraning("Failed to find loader.dll");
		return;
    }

#if _DEBUG
	LoadLoaderAssembly(loader_dll);
#else
    // Clear unwanted env vars
	SetEnvironmentVariableA("NODE_OPTIONS", NULL);
	SetEnvironmentVariableA("ELECTRON_NO_ASAR", NULL);
	SetEnvironmentVariableA("ELECTRON_RUN_AS_NODE", NULL);

    // In AOT mode, just load loader.dll directly
    HMODULE hLoader = LoadLibraryW(loader_dll.c_str());
	if (hLoader == nullptr) {
		ShowWraning("Failed to load loader.dll");
		return;
    }

    using NativeMain = void(*)();
    auto pNativeMain = (NativeMain)GetProcAddress(hLoader, "NativeMain");
    if (pNativeMain == nullptr) {
        ShowWraning("Failed to find NativeMain in loader.dll");
        return;
    }

    pNativeMain();
#endif
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