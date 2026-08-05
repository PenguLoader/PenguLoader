#include "doctest.h"

#include "browser/assets_range.h"

using assets::RangeParse;
using assets::parse_byte_range;

namespace
{
    // Wrapper so a case reads as one line. `total` is the entity length.
    struct Parsed
    {
        RangeParse result;
        int64_t start = -1;
        int64_t end = -1;
    };

    Parsed run(const char *header, int64_t total)
    {
        Parsed p;
        p.result = parse_byte_range(header, total, p.start, p.end);
        return p;
    }
}

TEST_CASE("byte ranges that resolve to a satisfiable window")
{
    SUBCASE("explicit start and end")
    {
        auto p = run("bytes=0-99", 1000);
        CHECK(p.result == RangeParse::Satisfiable);
        CHECK(p.start == 0);
        CHECK(p.end == 99);
    }

    SUBCASE("open-ended runs to the last byte")
    {
        auto p = run("bytes=500-", 1000);
        CHECK(p.result == RangeParse::Satisfiable);
        CHECK(p.start == 500);
        CHECK(p.end == 999);
    }

    // Regression: `rangeEnd == 0` used to double as the "to end of entity"
    // sentinel, so a single-first-byte request returned the whole file.
    SUBCASE("bytes=0-0 is the first byte, not the whole entity")
    {
        auto p = run("bytes=0-0", 1000);
        CHECK(p.result == RangeParse::Satisfiable);
        CHECK(p.start == 0);
        CHECK(p.end == 0);
    }

    SUBCASE("last byte of the entity")
    {
        auto p = run("bytes=999-999", 1000);
        CHECK(p.result == RangeParse::Satisfiable);
        CHECK(p.start == 999);
        CHECK(p.end == 999);
    }

    // Regression: suffix form used to fall through atoi into a negative seek,
    // emitting `Content-Range: bytes -500-500/1000`.
    SUBCASE("suffix form takes the final N bytes")
    {
        auto p = run("bytes=-500", 1000);
        CHECK(p.result == RangeParse::Satisfiable);
        CHECK(p.start == 500);
        CHECK(p.end == 999);
    }

    SUBCASE("suffix larger than the entity clamps to the whole entity")
    {
        auto p = run("bytes=-2000", 1000);
        CHECK(p.result == RangeParse::Satisfiable);
        CHECK(p.start == 0);
        CHECK(p.end == 999);
    }

    // Regression: the end used to be echoed unclamped, producing
    // `Content-Length: 99500` for a 1000-byte file.
    SUBCASE("end past the entity is clamped")
    {
        auto p = run("bytes=500-99999", 1000);
        CHECK(p.result == RangeParse::Satisfiable);
        CHECK(p.start == 500);
        CHECK(p.end == 999);
    }

    SUBCASE("the range unit is case-insensitive")
    {
        auto p = run("BYTES=0-9", 1000);
        CHECK(p.result == RangeParse::Satisfiable);
        CHECK(p.start == 0);
        CHECK(p.end == 9);
    }

    SUBCASE("optional whitespace is tolerated")
    {
        auto p = run("bytes= 0 - 99 ", 1000);
        CHECK(p.result == RangeParse::Satisfiable);
        CHECK(p.start == 0);
        CHECK(p.end == 99);
    }
}

TEST_CASE("byte ranges that are well-formed but unsatisfiable")
{
    // These must produce 416 + `Content-Range: bytes */<len>`, not a 206.
    SUBCASE("first-byte-pos at the end of the entity")
    {
        CHECK(run("bytes=1000-", 1000).result == RangeParse::Unsatisfiable);
    }

    SUBCASE("first-byte-pos beyond the entity")
    {
        CHECK(run("bytes=5000-6000", 1000).result == RangeParse::Unsatisfiable);
    }

    SUBCASE("a zero-length suffix asks for nothing")
    {
        CHECK(run("bytes=-0", 1000).result == RangeParse::Unsatisfiable);
    }

    SUBCASE("any range against an empty entity")
    {
        CHECK(run("bytes=0-", 0).result == RangeParse::Unsatisfiable);
        CHECK(run("bytes=-1", 0).result == RangeParse::Unsatisfiable);
    }
}

TEST_CASE("headers that must be ignored in favour of a full 200")
{
    // Regression: the old parser sliced off `bytes=` without checking for it,
    // so anything shorter than six characters threw std::out_of_range straight
    // out of a CEF callback.
    SUBCASE("shorter than the unit prefix does not throw")
    {
        CHECK(run("ab", 1000).result == RangeParse::Ignore);
        CHECK(run("x", 1000).result == RangeParse::Ignore);
        CHECK(run("", 1000).result == RangeParse::Ignore);
        CHECK(run("bytes=", 1000).result == RangeParse::Ignore);
    }

    SUBCASE("a unit we do not serve")
    {
        CHECK(run("items=0-99", 1000).result == RangeParse::Ignore);
        CHECK(run("bytes 0-99", 1000).result == RangeParse::Ignore);
    }

    SUBCASE("multi-range sets need multipart/byteranges, so decline them")
    {
        CHECK(run("bytes=0-99,200-299", 1000).result == RangeParse::Ignore);
    }

    // RFC 7233 2.1: last-byte-pos below first-byte-pos makes the spec invalid,
    // which is ignored rather than answered with a 416.
    SUBCASE("end below start is invalid, not unsatisfiable")
    {
        CHECK(run("bytes=99-0", 1000).result == RangeParse::Ignore);
    }

    SUBCASE("non-numeric positions")
    {
        CHECK(run("bytes=abc-1", 1000).result == RangeParse::Ignore);
        CHECK(run("bytes=0-xyz", 1000).result == RangeParse::Ignore);
        CHECK(run("bytes=--1", 1000).result == RangeParse::Ignore);
    }

    SUBCASE("no dash at all")
    {
        CHECK(run("bytes=12", 1000).result == RangeParse::Ignore);
    }

    // Guarding the arithmetic, not the syntax: a 20-digit run overflows int64
    // and a wrapped value would produce a bogus but *plausible* window.
    SUBCASE("numbers long enough to overflow are refused, not wrapped")
    {
        CHECK(run("bytes=99999999999999999999-", 1000).result == RangeParse::Ignore);
        CHECK(run("bytes=0-99999999999999999999", 1000).result == RangeParse::Ignore);
    }
}
