#pragma once

// Path-sandboxing + URI decoding helpers for the plugins scheme handler
// and the writable-JSON `$write` binding. Both code paths need to verify
// that a caller-supplied URL resolves to a filesystem path that stays inside
// an expected root directory — otherwise a plugin (or stray eval'd code)
// could read or overwrite files outside its scope via `..` traversal.

#include "pengu.h"   // for `path` alias and OS_* macros (`./src` is on the include path)
#include "path_guard.h"   // assets::is_inside — CEF-free, so it can be unit-tested
#include "include/capi/cef_parser_capi.h"

#include <cstring>
#include <string>

namespace assets
{
    /// Percent-decode a UTF-16 URL path in place. Path separators stay
    /// escaped (per UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS) so a `%2F`
    /// in a filename can't fake a directory split.
    inline void decode_uri(std::u16string &uri)
    {
        auto rule = cef_uri_unescape_rule_t(UU_SPACES
            | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

        cef_string_t input{ (char16 *)uri.data(), uri.length(), nullptr };
        CefScopedStr output{ cef_uridecode(&input, true, rule) };

        uri.assign((char16_t *)output.str, output.length);
    }
}
