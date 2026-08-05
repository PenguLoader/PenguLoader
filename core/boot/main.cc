#include "bootstrap.h"
#include "trust.h"
#include "trust_key.h"

#include <stdio.h>
#include <string>
#include <vector>

// =============================================================================
// boot.dll — the stable IFEO entry point, loaded by rundll32.
//
// IFEO's Debugger value names `rundll32 "<boot.dll>", #6000`, permanently, from
// an admin-only location. We resolve which Pengu runtime to load from a
// per-user pointer file and inject that, so:
//
//   * turning Pengu on or off is a file write, not an admin-gated registry
//     edit;
//   * deleting a portable install can't brick the client, because a stale
//     pointer just means no runtime;
//   * one machine-wide IFEO key no longer forces one install on every account.
//
// A DLL under rundll32 rather than an executable of our own, because Defender
// quarantines the executable. Registering an unknown binary as an IFEO
// Debugger is a persistence technique in its own right, and it is scored
// behaviourally at the moment the client triggers it -- Authenticode does not
// buy an exemption. rundll32 is Microsoft-signed and is the shape this project
// shipped for years before the executable existed.
//
// The invariant that matters more than any of it:
//
//     LeagueClientUx.exe always launches.
//
// Every failure below falls through to a plain launch. Losing the plugin
// runtime is an inconvenience; failing to start the game client is not.
// =============================================================================

namespace
{
    namespace fs = std::filesystem;

    /// `%LOCALAPPDATA%\.pengu\active` — resolved from the environment of this
    /// process, which IFEO started as whoever launched the client. That's what
    /// makes a machine-wide IFEO key produce per-user activation.
    fs::path active_pointer_path()
    {
        wchar_t buf[2048];
        DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, _countof(buf));
        if (length == 0 || length >= _countof(buf))
            return {};

        return fs::path(buf) / L".pengu" / L"active";
    }

    /// Read the pointer file. Contents are the absolute directory of the
    /// install to load, one line, UTF-8. Absent, empty, or unreadable all mean
    /// "not activated for this user".
    std::wstring read_active(const fs::path &file)
    {
        HANDLE h = CreateFileW(file.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);

        if (h == INVALID_HANDLE_VALUE)
            return {};

        // A path can't legitimately be longer than this, and the file is
        // user-writable, so cap the read rather than trusting its size.
        char raw[4096];
        DWORD read = 0;
        BOOL ok = ReadFile(h, raw, sizeof(raw) - 1, &read, NULL);
        CloseHandle(h);

        if (!ok || read == 0)
            return {};

        raw[read] = '\0';

        // First line only; trim trailing whitespace.
        std::string line(raw, read);
        if (auto cut = line.find_first_of("\r\n"); cut != std::string::npos)
            line.resize(cut);
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t'))
            line.pop_back();

        if (line.empty())
            return {};

        int wide = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, nullptr, 0);
        if (wide <= 0)
            return {};

        std::wstring out(static_cast<size_t>(wide) - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, out.data(), wide);
        return out;
    }

    /// Record why we didn't inject, beside the pointer, so the hub can tell
    /// the user rather than leaving Pengu to fail silently. A verification
    /// failure is far more likely to be a rotation mistake than an attack and
    /// should read that way.
    void report(const wchar_t *reason)
    {
        auto pointer = active_pointer_path();
        if (pointer.empty())
            return;

        auto log = pointer.parent_path() / L"boot.log";

        HANDLE h = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
            return;

        char line[512];
        int n = _snprintf_s(line, _TRUNCATE, "%ls\r\n", reason);
        if (n > 0)
        {
            DWORD written = 0;
            WriteFile(h, line, static_cast<DWORD>(n), &written, NULL);
        }

        CloseHandle(h);
    }

    /// A runtime we've decided to trust, with the handle that proved it still
    /// open. Holding the handle across injection is the point: it was opened
    /// denying write sharing, so the bytes we hashed and verified are the
    /// bytes that stay on disk while LoadLibraryW reads them.
    struct Runtime
    {
        fs::path core;
        HANDLE   file = INVALID_HANDLE_VALUE;

        ~Runtime()
        {
            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
    };

    /// Turn the pointer's contents into a verified runtime, or leave
    /// `core` empty if anything about it doesn't hold up.
    void resolve_runtime(Runtime &out)
    {
        auto pointer = active_pointer_path();
        if (pointer.empty())
            return;

        auto dir = read_active(pointer);
        if (dir.empty())
            return; // not activated for this user — the quiet, normal case

        fs::path root(dir);

        // Must be absolute: a relative pointer would resolve against whatever
        // working directory the launcher happened to leave us in.
        if (!root.is_absolute())
        {
            report(L"active pointer is not an absolute path");
            return;
        }

        auto core = (root / L"core.dll").lexically_normal();

        // Deny write sharing for as long as we hold this. Everything after
        // this point verifies the handle, not the path.
        HANDLE file = CreateFileW(core.c_str(), GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (file == INVALID_HANDLE_VALUE)
        {
            report(L"no core.dll at the path named by the active pointer");
            return;
        }

        auto result = trust::verify(file, core);
        if (result != trust::Result::Ok)
        {
            wchar_t reason[512];
            _snwprintf_s(reason, _TRUNCATE, L"%hs (%ls)",
                trust::describe(result), core.c_str());
            report(reason);
            CloseHandle(file);
            return;
        }

        out.core = std::move(core);
        out.file = file;
    }

    /// Drop a stale refusal once a launch succeeds.
    ///
    /// report() truncates and is never called on the happy path, so without
    /// this the last failure stays in the file — and on the hub's Settings
    /// page — long after the release that caused it was replaced. A refusal
    /// people can't clear reads as a bug in Pengu.
    void forget_refusal()
    {
        auto pointer = active_pointer_path();
        if (!pointer.empty())
            DeleteFileW((pointer.parent_path() / L"boot.log").c_str());
    }
}

// The rundll32 entry point, exported at ordinal 6000.
//
// rundll32 hands the entry point only the arguments that follow the DLL and
// entry-point tokens, so what arrives here is already the original
// LeagueClientUx.exe command line -- no argv[0] of our own to strip.
//
// Invoked by ordinal rather than by name, matching the value this project has
// shipped since v1.1.6. Worth knowing that the ordinal form gives rundll32 no
// way to signal ANSI versus Unicode and it picks Unicode; a name would let it
// resolve `<name>W` explicitly, but the ordinal is the form with years of
// production behind it and the signature below is what it has always been
// called with.
extern "C" int APIENTRY _BootstrapEntry(HWND, HINSTANCE, LPWSTR commandLine, int)
{
    if (commandLine == nullptr || *commandLine == L'\0')
    {
        // Launched directly by a person rather than by IFEO. Nothing sensible
        // to do, and silently doing nothing is friendlier than a crash.
        MessageBoxW(NULL,
            L"This program is started automatically by Windows when the League client "
            L"launches. There is nothing to do here.",
            L"Pengu Loader", MB_ICONINFORMATION | MB_OK);
        return 0;
    }

    if constexpr (PENGU_TRUST_KEY_IS_DEV)
        report(L"WARNING: built with the development trust key");

    // Empty runtime path == passthrough: launch the client with nothing
    // injected. Every failure in resolve_runtime() lands here, and the handle
    // stays open across launch() so nothing can swap the file underneath us.
    Runtime runtime;
    resolve_runtime(runtime);

    if (!runtime.core.empty())
        forget_refusal();

    // CreateProcessW writes into the buffer it is handed, and this one belongs
    // to rundll32.
    std::vector<wchar_t> buffer(commandLine, commandLine + wcslen(commandLine) + 1);

    return bootstrap::launch(buffer.data(), runtime.core);
}
