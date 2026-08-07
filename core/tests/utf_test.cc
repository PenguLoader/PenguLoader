#include "doctest.h"

#include "utf.h"

#include <string>

// The UTF-16 -> UTF-8 encoder behind CefStrBase::to_utf8. It replaced a
// cef_string_to_utf8 call, so it carries every string that crosses the V8
// boundary: PluginFS paths and file content, DataStore blobs, `$write` bodies,
// RCS credentials, Range headers. A miscount here is a heap overrun, and a
// mis-encode silently corrupts a plugin's saved data.
//
// Source stays ASCII-only and spells scalars as \u escapes: the build does not
// pass /utf-8, so a literal non-ASCII character here would be read in the
// system codepage and the expectations would drift per machine.

using utf::utf16_to_utf8;
using utf::utf8_length;

// Expected encodings, as named constants so no \x escape ever sits next to a
// hex digit (where it would be swallowed into a longer escape).
static const std::string E_0080   = "\xC2\x80";              // U+0080
static const std::string E_00E9   = "\xC3\xA9";              // U+00E9 e-acute
static const std::string E_07FF   = "\xDF\xBF";              // U+07FF
static const std::string E_0800   = "\xE0\xA0\x80";          // U+0800
static const std::string E_20AC   = "\xE2\x82\xAC";          // U+20AC euro
static const std::string E_4E2D   = "\xE4\xB8\xAD";          // U+4E2D CJK
static const std::string E_FFFD   = "\xEF\xBF\xBD";          // U+FFFD replacement
static const std::string E_FFFF   = "\xEF\xBF\xBF";          // U+FFFF
static const std::string E_10000  = "\xF0\x90\x80\x80";      // U+10000
static const std::string E_1F600  = "\xF0\x9F\x98\x80";      // U+1F600 emoji
static const std::string E_10FFFF = "\xF4\x8F\xBF\xBF";      // U+10FFFF

/// Encode and assert the two loops agreed -- utf8_length sizes the buffer that
/// the encoder then fills, so any divergence is an overrun in production.
static std::string enc(std::u16string_view src)
{
    std::string out;
    utf16_to_utf8(src.data(), src.size(), out);
    CHECK(out.size() == utf8_length(src.data(), src.size()));
    return out;
}

TEST_CASE("ascii and empty input")
{
    CHECK(enc(u"") == "");
    CHECK(enc(u"hello") == "hello");
    CHECK(enc(u"{\"a\":1}") == "{\"a\":1}");

    // Every ASCII value, including the control range.
    std::u16string all;
    for (char16_t c = 1; c < 0x80; ++c)
        all.push_back(c);

    const std::string out = enc(all);
    REQUIRE(out.size() == 127);
    CHECK(static_cast<unsigned char>(out.front()) == 1);
    CHECK(static_cast<unsigned char>(out.back()) == 0x7F);
}

TEST_CASE("length is authoritative, so embedded NULs survive")
{
    const std::u16string src{ u'a', u'\0', u'b' };
    const std::string out = enc(src);

    REQUIRE(out.size() == 3);
    CHECK(out[0] == 'a');
    CHECK(out[1] == '\0');
    CHECK(out[2] == 'b');
}

TEST_CASE("multi-byte scalars")
{
    SUBCASE("two-byte")
    {
        CHECK(enc(u"\u0080") == E_0080);
        CHECK(enc(u"\u00E9") == E_00E9);
        CHECK(enc(u"\u07FF") == E_07FF);
    }

    SUBCASE("three-byte")
    {
        CHECK(enc(u"\u0800") == E_0800);
        CHECK(enc(u"\u20AC") == E_20AC);
        CHECK(enc(u"\u4E2D") == E_4E2D);
        CHECK(enc(u"\uFFFF") == E_FFFF);
    }

    SUBCASE("four-byte, from surrogate pairs")
    {
        CHECK(enc(std::u16string{ u'\xD800', u'\xDC00' }) == E_10000);
        CHECK(enc(std::u16string{ u'\xD83D', u'\xDE00' }) == E_1F600);
        CHECK(enc(std::u16string{ u'\xDBFF', u'\xDFFF' }) == E_10FFFF);
    }
}

TEST_CASE("boundary values encode at the expected width")
{
    CHECK(enc(u"\u007F").size() == 1);
    CHECK(enc(u"\u0080").size() == 2);
    CHECK(enc(u"\u07FF").size() == 2);
    CHECK(enc(u"\u0800").size() == 3);
    CHECK(enc(u"\uFFFF").size() == 3);
    CHECK(enc(std::u16string{ u'\xD800', u'\xDC00' }).size() == 4);
}

// `'\uD800'` is a legal JS string literal, so lone surrogates reach us from
// real plugin code. Emitting them raw would produce CESU-8 -- not valid UTF-8,
// and rejected or mangled by anything that reads the file back.
TEST_CASE("lone surrogates become U+FFFD")
{
    SUBCASE("lone lead")
        CHECK(enc(std::u16string{ u'\xD800' }) == E_FFFD);

    SUBCASE("lone trail")
        CHECK(enc(std::u16string{ u'\xDC00' }) == E_FFFD);

    SUBCASE("lead at end of input, with no room for a trail")
        CHECK(enc(std::u16string{ u'a', u'\xD83D' }) == "a" + E_FFFD);

    SUBCASE("lead followed by a non-surrogate")
        CHECK(enc(std::u16string{ u'\xD83D', u'b' }) == E_FFFD + "b");

    SUBCASE("lead followed by another lead")
        CHECK(enc(std::u16string{ u'\xD83D', u'\xD83D' }) == E_FFFD + E_FFFD);

    SUBCASE("trail then lead -- a reversed pair, neither half is valid")
        CHECK(enc(std::u16string{ u'\xDE00', u'\xD83D' }) == E_FFFD + E_FFFD);

    SUBCASE("a valid pair still encodes after a lone surrogate")
        CHECK(enc(std::u16string{ u'\xDC00', u'\xD83D', u'\xDE00' }) == E_FFFD + E_1F600);
}

TEST_CASE("mixed content round-trips in one pass")
{
    // 1, 2, 3 and 4 byte scalars interleaved with ASCII.
    const std::u16string src = std::u16string(u"a\u00E9b\u20ACc")
                             + std::u16string{ u'\xD83D', u'\xDE00' }
                             + u"d";

    CHECK(enc(src) == "a" + E_00E9 + "b" + E_20AC + "c" + E_1F600 + "d");
}

TEST_CASE("the destination buffer is replaced, not appended to")
{
    std::string reused = "stale content that is much longer than the new value";
    const auto before = reused.capacity();

    utf16_to_utf8(u"hi", 2, reused);
    CHECK(reused == "hi");

    // The point of the _into form: a shorter follow-up conversion reuses the
    // allocation rather than making a new one.
    CHECK(reused.capacity() == before);

    utf16_to_utf8(u"", 0, reused);
    CHECK(reused.empty());
}

TEST_CASE("null source is treated as empty")
{
    std::string out = "previous";
    utf16_to_utf8(nullptr, 0, out);
    CHECK(out.empty());
}

TEST_CASE("large input is sized in one allocation and encoded whole")
{
    // Exercises the two-pass sizing at a scale where an off-by-one in
    // utf8_length corrupts the heap rather than failing quietly.
    const std::u16string unit = std::u16string(u"a\u00E9\u20AC")
                              + std::u16string{ u'\xD83D', u'\xDE00' };

    std::u16string src;
    for (int i = 0; i < 10000; ++i)
        src += unit;

    const std::string expected_unit = "a" + E_00E9 + E_20AC + E_1F600;
    const size_t expected = 10000 * expected_unit.size();

    CHECK(expected_unit.size() == 1 + 2 + 3 + 4);
    CHECK(utf8_length(src.data(), src.size()) == expected);

    const std::string out = enc(src);
    REQUIRE(out.size() == expected);
    CHECK(out.compare(0, expected_unit.size(), expected_unit) == 0);
    CHECK(out.compare(expected - expected_unit.size(), expected_unit.size(), expected_unit) == 0);
}
