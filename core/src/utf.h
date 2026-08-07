#pragma once

// UTF-16 -> UTF-8 transcoding. Split out of cefstr.cc so it can be unit-tested:
// everything here is pointer -> buffer with no CEF, no platform and no
// filesystem dependency.
//
// This exists instead of `cef_string_to_utf8` because that function always
// allocates its own buffer, so every conversion cost an allocation, a full
// memcpy into the destination std::string, and a free. On the PluginFS paths
// that runs over payloads up to 16 MB. Here the caller owns the buffer and the
// bytes are written into it once.

#include <cstddef>
#include <string>

namespace utf
{
    inline constexpr char32_t REPLACEMENT = 0xFFFD;

    inline bool is_lead_surrogate(char32_t unit)
    {
        return unit >= 0xD800 && unit <= 0xDBFF;
    }

    inline bool is_trail_surrogate(char32_t unit)
    {
        return unit >= 0xDC00 && unit <= 0xDFFF;
    }

    ///
    /// Exact number of bytes `utf16_to_utf8` will write for [src, src+length).
    ///
    /// Kept in lockstep with the encoder below -- the two loops must classify
    /// every unit the same way or the encoder overruns its buffer. The unit
    /// test asserts the invariant directly rather than trusting inspection.
    ///
    inline size_t utf8_length(const char16_t *src, size_t length)
    {
        size_t bytes = 0;

        for (size_t i = 0; i < length; ++i)
        {
            char32_t unit = src[i];

            if (unit < 0x80)
                bytes += 1;
            else if (unit < 0x800)
                bytes += 2;
            else if (is_lead_surrogate(unit) && i + 1 < length && is_trail_surrogate(src[i + 1]))
            {
                bytes += 4;
                ++i;    // the trail belongs to this lead, not to itself
            }
            else
                bytes += 3;     // BMP scalar, or a lone surrogate -> U+FFFD
        }

        return bytes;
    }

    ///
    /// Transcode [src, src+length) into `to`, replacing whatever it held.
    ///
    /// `to`'s existing capacity is reused, so a caller converting repeatedly
    /// can keep one buffer around instead of allocating per call.
    ///
    /// Embedded NULs are preserved -- `length` is authoritative, not a
    /// terminator.
    ///
    inline void utf16_to_utf8(const char16_t *src, size_t length, std::string &to)
    {
        to.clear();

        if (src == nullptr || length == 0)
            return;

        to.resize(utf8_length(src, length));
        char *out = to.data();

        for (size_t i = 0; i < length; ++i)
        {
            char32_t cp = src[i];

            if (cp < 0x80)
            {
                *out++ = static_cast<char>(cp);
                continue;
            }

            if (cp < 0x800)
            {
                *out++ = static_cast<char>(0xC0 | (cp >> 6));
                *out++ = static_cast<char>(0x80 | (cp & 0x3F));
                continue;
            }

            if (is_lead_surrogate(cp) && i + 1 < length && is_trail_surrogate(src[i + 1]))
            {
                char32_t trail = src[i + 1];
                cp = 0x10000 + ((cp - 0xD800) << 10) + (trail - 0xDC00);
                ++i;

                *out++ = static_cast<char>(0xF0 | (cp >> 18));
                *out++ = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                *out++ = static_cast<char>(0x80 | (cp & 0x3F));
                continue;
            }

            // A surrogate with no partner. JS strings are arbitrary UTF-16
            // sequences, not well-formed Unicode, so this arrives from real
            // plugin code -- `'\uD800'` is a valid string literal. Emitting it
            // raw would produce CESU-8, which is not valid UTF-8 and would be
            // rejected or mangled by anything reading the file back.
            //
            // U+FFFD is what CEF's own lenient conversion substitutes, and it
            // is 3 bytes like the BMP case below, so utf8_length stays in step.
            if (is_lead_surrogate(cp) || is_trail_surrogate(cp))
                cp = REPLACEMENT;

            *out++ = static_cast<char>(0xE0 | (cp >> 12));
            *out++ = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            *out++ = static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
}
