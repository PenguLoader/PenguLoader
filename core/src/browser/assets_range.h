#pragma once

// RFC 7233 `Range` header parsing for the `https://plugins/` scheme handler.
//
// Deliberately free of any CEF or Windows dependency: this is pure string ->
// (start, end) arithmetic, so it can be unit-tested without a client, a CEF
// checkout, or a platform. Everything that needs CEF stays in assets.cc.

#include <cstdint>
#include <string>

namespace assets
{
    // Outcome of parsing a `Range` header, per RFC 7233.
    enum class RangeParse
    {
        // Not a byte range we can act on - unknown unit, malformed, or a
        // multi-range set. RFC 7233 3.1 lets the server ignore it and reply 200.
        Ignore,
        // Well-formed, but nothing in it overlaps the entity. Reply 416.
        Unsatisfiable,
        // Usable. `start` / `end` are inclusive and clamped to the entity.
        Satisfiable,
    };

    // Parse a decimal run. Rejects empty input, non-digits, and anything long
    // enough to overflow - callers treat all three as "ignore the header" rather
    // than trusting a wrapped value.
    inline bool parse_range_number(const std::string &text, int64_t &out)
    {
        if (text.empty() || text.size() > 18)
            return false;

        int64_t value = 0;
        for (char c : text)
        {
            if (c < '0' || c > '9')
                return false;
            value = value * 10 + (c - '0');
        }

        out = value;
        return true;
    }

    // Strip optional whitespace (RFC 7230 OWS) from both ends.
    inline void trim_ows(std::string &text)
    {
        size_t begin = text.find_first_not_of(" \t");
        if (begin == std::string::npos)
        {
            text.clear();
            return;
        }

        size_t end = text.find_last_not_of(" \t");
        text = text.substr(begin, end - begin + 1);
    }

    ///
    /// Parse a single byte-range-spec out of a `Range` header value.
    ///
    /// Only one range is supported: answering a multi-range set requires a
    /// multipart/byteranges body, and nothing in LCUX asks for one, so those are
    /// ignored in favour of the full entity.
    ///
    inline RangeParse parse_byte_range(const std::string &header,
        int64_t total, int64_t &start, int64_t &end)
    {
        static constexpr char UNIT[] = "bytes=";
        constexpr size_t UNIT_LEN = sizeof(UNIT) - 1;

        // Range units are case-insensitive (RFC 7233 2). Note this also guards the
        // substr below - a header shorter than the unit prefix would otherwise
        // throw std::out_of_range straight out of a CEF callback.
        if (header.size() <= UNIT_LEN)
            return RangeParse::Ignore;

        for (size_t i = 0; i < UNIT_LEN; i++)
        {
            char c = header[i];
            if (c >= 'A' && c <= 'Z') c += 32;
            if (c != UNIT[i])
                return RangeParse::Ignore;
        }

        std::string spec = header.substr(UNIT_LEN);

        // A comma means a multi-range set.
        if (spec.find(',') != std::string::npos)
            return RangeParse::Ignore;

        size_t dash = spec.find('-');
        if (dash == std::string::npos)
            return RangeParse::Ignore;

        std::string first = spec.substr(0, dash);
        std::string last = spec.substr(dash + 1);
        trim_ows(first);
        trim_ows(last);

        if (first.empty())
        {
            // Suffix form `bytes=-N` - the final N bytes of the entity.
            int64_t count;
            if (!parse_range_number(last, count))
                return RangeParse::Ignore;

            // `bytes=-0` asks for the last zero bytes: valid syntax, nothing to
            // send.
            if (count == 0 || total == 0)
                return RangeParse::Unsatisfiable;

            start = count >= total ? 0 : total - count;
            end = total - 1;
            return RangeParse::Satisfiable;
        }

        if (!parse_range_number(first, start))
            return RangeParse::Ignore;

        if (last.empty())
        {
            // Open-ended `bytes=N-` runs to the end of the entity. This is the case
            // the old sentinel (`rangeEnd == 0`) conflated with `bytes=0-0`.
            end = total - 1;
        }
        else
        {
            if (!parse_range_number(last, end))
                return RangeParse::Ignore;

            // last-byte-pos below first-byte-pos makes the spec *invalid* rather
            // than unsatisfiable (RFC 7233 2.1), so it gets ignored, not 416'd.
            if (end < start)
                return RangeParse::Ignore;

            if (end > total - 1)
                end = total - 1;
        }

        // A first-byte-pos at or past the end has nothing behind it.
        if (total == 0 || start >= total)
            return RangeParse::Unsatisfiable;

        return RangeParse::Satisfiable;
    }
}
