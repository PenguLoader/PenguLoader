#pragma once

// Shared constants for the `https://plugins/` scheme handler.
// Lives in a header because the upcoming writable-JSON module will add
// SCRIPT_IMPORT_JSON_WRITABLE here without bloating assets.cc.

#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace assets
{
    // FNV-1a 32-bit. Templated so the same routine hashes char (for the
    // user-defined literal used at compile time) and char16_t (for the
    // runtime extension lookup from std::u16string).
    template <typename T>
    constexpr uint32_t fnv32_1a(const T *in, size_t len)
    {
        uint32_t hash = 2166136261u;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint32_t>(static_cast<unsigned char>(in[i]));
            hash *= 16777619u;
        }
        return hash;
    }
}

// Global UDL: `"png"_hash` evaluates at compile time. Kept at global scope
// for ergonomics — initializing KNOWN_ASSETS_SET below uses it dozens of times.
inline constexpr uint32_t operator""_hash(const char *in, size_t len)
{
    return assets::fnv32_1a(in, len);
}

namespace assets
{
    // Hashed extensions that the JSON / CSS / RAW / URL switch in _open
    // treats as "asset URL" imports (`import logo from './logo.png'`
    // produces a URL string).
    inline const std::unordered_set<uint32_t> KNOWN_ASSETS_SET
    {
        // images
        "bmp"_hash,  "png"_hash,
        "jpg"_hash,  "jpeg"_hash, "jfif"_hash,
        "pjpeg"_hash,"pjp"_hash,  "gif"_hash,
        "svg"_hash,  "ico"_hash,  "webp"_hash,
        "avif"_hash,

        // media
        "mp4"_hash,  "webm"_hash,
        "ogg"_hash,  "mp3"_hash,  "wav"_hash,
        "flac"_hash, "aac"_hash,

        // fonts
        "woff"_hash, "woff2"_hash,
        "eot"_hash,  "ttf"_hash,  "otf"_hash,
    };

    // ESM-style import shims served when `?url` / `?raw` is requested or
    // when the file extension matches a known module type.
    //
    // NOTE: SCRIPT_IMPORT_CSS relies on the late-listener polyfill in
    // packages/preload/src/preload/load-hooks.ts — if readyState is already
    // 'interactive' when the import runs, DOMContentLoaded has fired and the
    // raw addEventListener would never resolve without that polyfill.

    inline constexpr const char *SCRIPT_IMPORT_CSS = R"(
(async function () {
    if (document.readyState !== 'complete')
        await new Promise(res => document.addEventListener('DOMContentLoaded', res));

    const url = import.meta.url.replace(/\?.*$/, '');
    const link = document.createElement('link');
    link.setAttribute('rel', 'stylesheet');
    link.setAttribute('href', url);

    document.body.appendChild(link);
})();
)";

    // Parsed JSON object/array gets a non-enumerable `$write` bound to the
    // module's own URL. Calling `data.$write()` returns a Promise that
    // resolves when an atomic temp+rename write is durable on disk.
    //
    // `window.__pwj` is captured from `window.__native.WriteJson` by the
    // preload (api/json.ts) before any plugin imports JSON — see the same
    // late-listener tradeoff documented for SCRIPT_IMPORT_CSS.
    //
    // NOTE: `$write` deliberately passes NO path. The native side derives the
    // target from the calling script's own URL via the V8 stack, so this shim
    // can only ever rewrite the .json module it was served for. Passing a URL
    // would make `window.__pwj` an ambient write-anywhere-in-plugins
    // capability for every script in the renderer, remote ones included.
    // See v8_json_write.cc.
    //
    // Primitives (e.g. JSON `true`, `42`, `"hi"`) can't have properties, so
    // `$write` is only attached to object/array roots. Plugin authors who
    // want $write should ship `{...}` or `[...]` JSON, which is overwhelmingly
    // the common case for config files.
    // `$write(space?)` mirrors JSON.stringify's third argument:
    //   `$write()`     → compact JSON
    //   `$write(2)`    → 2-space indent
    //   `$write('\t')` → tab indent
    inline constexpr const char *SCRIPT_IMPORT_JSON = R"(
const url = import.meta.url.replace(/\?.*$/, '');
const content = await fetch(url).then(r => r.text());
const data = JSON.parse(content);

if (data !== null && typeof data === 'object') {
    Object.defineProperty(data, '$write', {
        value: (space) => window.__pwj(JSON.stringify(data, null, space)),
        writable: false,
        configurable: false,
        enumerable: false,
    });
}

export default data;
)";

    inline constexpr const char *SCRIPT_IMPORT_RAW = R"(
const url = import.meta.url.replace(/\?.*$/, '');
const content = await fetch(url).then(r => r.text());
export default content;
)";

    inline constexpr const char *SCRIPT_IMPORT_URL = R"(
const url = import.meta.url.replace(/\?.*$/, '');
export default url;
)";
}
