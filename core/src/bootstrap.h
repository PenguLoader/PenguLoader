#pragma once

// Windows-only. The macOS makefile globs src/*.cc and src/*.h, so this guards
// itself rather than relying on the build to exclude it — an unguarded header
// here takes dllmain.o down with it.
//
// _WIN32 rather than pengu.h's OS_WIN on purpose: boot.dll links this and
// nothing else from core, and pulling in pengu.h would drag CEF along.
#ifdef _WIN32

// The IFEO bootstrap sequence, shared by core.dll's ordinal-6000 export and
// boot.dll's.
//
// Deliberately free of pengu.h / CEF includes: boot.dll links this and nothing
// else from core, so the boot stays a few tens of KB and has no CEF surface.
// Keep it that way.

#include <windows.h>
#include <filesystem>

namespace bootstrap
{
    /// Launch the real LeagueClientUx.exe from `commandLine` and, if
    /// `injectDll` is non-empty, load it into the new process before the
    /// client's own code runs.
    ///
    /// Always creates the process. Injection is best-effort: an unreadable or
    /// unloadable DLL costs the plugin runtime, never the client launch —
    /// LeagueClientUx.exe must launch no matter what goes wrong here.
    ///
    /// `commandLine` must be a mutable buffer — CreateProcessW writes to it.
    ///
    /// @returns the exit code to return from the host process.
    int launch(wchar_t *commandLine, const std::filesystem::path &injectDll);

    /// Load `dll` into an already-created process via CreateRemoteThread +
    /// LoadLibraryW. Used by launch() and by core's CreateProcessW hook for
    /// renderer children.
    bool inject(HANDLE process, const std::filesystem::path &dll);
}

#endif // _WIN32
