#pragma once

// Path-sandboxing primitive shared by the plugins scheme handler, the `?dir`
// module and the writable-JSON `$write` binding. All three take a
// caller-supplied path and must prove it stays inside an expected root, or a
// plugin could read or overwrite files outside its scope via `..` traversal.
//
// Deliberately free of any CEF dependency so it can be unit-tested without a
// CEF checkout. `assets_path.h` pulls this in alongside the URI decoding that
// does need CEF.

#include <cstring>
#include <filesystem>

#ifdef _WIN32
#   include <cwchar>
#endif

namespace assets
{
    /// Returns true if the lexically-normalized `candidate` is contained
    /// within the lexically-normalized `root` (or is identical to it).
    ///
    /// Uses `lexically_normal` (string-only — no FS calls) so it's cheap
    /// to call on every request. Symlinks inside the path are NOT resolved;
    /// Pengu's data root doesn't use symlinks in any supported setup.
    ///
    /// On Windows the comparison is case-insensitive (NTFS default).
    /// On macOS it's case-sensitive (matches APFS / case-sensitive HFS+).
    inline bool is_inside(const std::filesystem::path &root,
                          const std::filesystem::path &candidate)
    {
        std::filesystem::path c = candidate.lexically_normal();
        c.make_preferred();
        std::filesystem::path r = root.lexically_normal();
        r.make_preferred();

        const auto &cs = c.native();
        const auto &rs = r.native();

        if (cs.length() < rs.length())
            return false;

#ifdef _WIN32
        if (_wcsnicmp(cs.data(), rs.data(), rs.length()) != 0)
            return false;
#else
        if (std::memcmp(cs.data(), rs.data(), rs.length() * sizeof(rs[0])) != 0)
            return false;
#endif

        // After matching the root prefix, the candidate must either end there
        // (candidate == root) or have a separator next — otherwise we'd accept
        // `<root>foo` as inside `<root>`.
        if (cs.length() > rs.length()
            && cs[rs.length()] != std::filesystem::path::preferred_separator)
            return false;

        return true;
    }
}
