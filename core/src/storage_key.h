#pragma once

// Deriving a plugin's storage database filename from its identity.
//
// Split out so it can be unit-tested: pure string -> string, no CEF, no
// filesystem. See docs/plugin-storage.md section 5.
//
// The identity is the same one `disabled_plugins` uses — the forward-slashed
// entry path relative to the plugins root, with any legacy trailing `_`
// stripped:
//
//     @nomi-san/snooze-css/index.js
//     indie-theme.js
//
// The *hash* is deliberately not the same. `disabled_plugins` uses FNV-1a
// 32-bit, which has no collision resistance — given a target value a colliding
// string can be constructed in milliseconds. There that is harmless: a
// collision toggles the wrong plugin, the list is user-controlled, and the
// failure is visible. Here it would be a cross-plugin data breach, since an
// author could name a repo so their store resolves to a popular plugin's file.
// See docs/plugin-storage.md section 5.2.

#include "sha256.h"

#include <string>
#include <string_view>

namespace storage
{
    /// Bytes of digest kept. 128 bits puts the birthday bound at 2^64, which
    /// is not a number reached by accident or on purpose.
    constexpr size_t KEY_BYTES = 16;

    /// Canonical form of a plugin identity, as hashed.
    ///
    /// Backslashes become forward slashes (renderer.cc builds entries with
    /// std::filesystem::path joins, so on Windows they arrive backslashed), a
    /// single trailing `_` is dropped (the v1.1.6 rename-to-disable
    /// convention), and the whole thing is lowercased.
    ///
    /// Lowercasing is ASCII-only, deliberately. Windows and default macOS
    /// filesystems are case-insensitive, so the same plugin can present with
    /// different casing and would otherwise hash to two different files —
    /// silently orphaning a store the user still has data in. A locale-aware
    /// tolower would be worse than useless here: under a Turkish locale 'I'
    /// lowercases to a dotless 'ı', so the same plugin would hash differently
    /// depending on the user's regional settings.
    inline std::string canonical(std::string_view path)
    {
        std::string out;
        out.reserve(path.size());

        for (char raw : path)
        {
            unsigned char ch = static_cast<unsigned char>(raw);
            if (ch == '\\')
                ch = '/';
            else if (ch >= 'A' && ch <= 'Z')
                ch = static_cast<unsigned char>(ch - 'A' + 'a');

            out.push_back(static_cast<char>(ch));
        }

        if (!out.empty() && out.back() == '_')
            out.pop_back();

        return out;
    }

    /// The 32-character hex stem of a plugin's database.
    inline std::string db_stem(std::string_view plugin_path)
    {
        const std::string key = canonical(plugin_path);
        return sha256::hex(key.data(), key.size(), KEY_BYTES);
    }

    /// The database filename, e.g. `3f0a1c9e5b7d2648a1e4c07b9d3f5182.db`.
    ///
    /// Flat, not nested: nothing has to create a directory before the first
    /// write, no `@` or `/` reaches the filesystem, and enumeration is one
    /// read_dir. The file is opaque by design — the database carries a `meta`
    /// table naming its owner, which survives the file being copied or
    /// renamed in a way a directory name would not.
    inline std::string db_name(std::string_view plugin_path)
    {
        return db_stem(plugin_path) + ".db";
    }
}
