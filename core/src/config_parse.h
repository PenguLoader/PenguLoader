#pragma once

// Pure parsing helpers for the `config` file. Split out of config.cc so they
// can be unit-tested: everything here is string -> value with no filesystem,
// no CEF and no platform dependency. config.cc keeps the file reading, the
// caching, and the option getters.

#include <cctype>
#include <string>

namespace config::parse
{
    /// Trim spaces, tabs, CR and LF from both ends, in place.
    ///
    /// CR matters because std::getline strips \n but leaves \r intact, so any
    /// line read from a CRLF-saved file (e.g. edited in Notepad) carries a
    /// trailing \r that would break value comparisons downstream
    /// ("true\r" != "true").
    inline void trim(std::string &str)
    {
        static constexpr const char *ws = " \t\r\n";
        auto last = str.find_last_not_of(ws);
        if (last == std::string::npos) { str.clear(); return; }
        str.erase(last + 1);
        str.erase(0, str.find_first_not_of(ws));
    }

    /// ASCII case-insensitive comparison against a NUL-terminated literal.
    inline bool iequals(const std::string &a, const char *b)
    {
        size_t bl = 0; while (b[bl]) ++bl;
        if (a.size() != bl) return false;
        for (size_t i = 0; i < bl; i++)
            if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
                return false;
        return true;
    }

    /// Split one ini line into key/value. Returns false for lines that carry
    /// no assignment — blanks, `;`/`#` comments, `[section]` headers (keys are
    /// globally unique, so sections are informational), and anything without
    /// an `=`. Key and value come back trimmed.
    inline bool ini_line(const std::string &raw, std::string &key, std::string &value)
    {
        std::string line = raw;
        // Trim first so leading whitespace doesn't hide the comment marker /
        // section bracket / key=value structure.
        trim(line);

        if (line.empty()) return false;
        if (line[0] == ';' || line[0] == '#') return false;
        if (line[0] == '[' && line.back() == ']') return false;

        size_t pos = line.find('=');
        if (pos == std::string::npos) return false;

        key = line.substr(0, pos);
        value = line.substr(pos + 1);

        trim(key);
        trim(value);

        return !key.empty();
    }

    /// Match the host-side IniReader.ParseBool surface so values written by
    /// either side round-trip cleanly: 1/0, true/false, yes/no, all
    /// case-insensitive. Anything else yields the fallback rather than
    /// guessing.
    inline bool as_bool(const std::string &value, bool fallback)
    {
        if (value == "1" || iequals(value, "true")  || iequals(value, "yes")) return true;
        if (value == "0" || iequals(value, "false") || iequals(value, "no"))  return false;
        return fallback;
    }

    /// std::stoi throws on bad input; a malformed number must not bring the
    /// core down, so anything unparseable yields the fallback.
    inline int as_int(const std::string &value, int fallback)
    {
        try { return std::stoi(value); }
        catch (...) { return fallback; }
    }
}
