#include "doctest.h"

#include "config_parse.h"

namespace cp = config::parse;

TEST_CASE("trim removes surrounding whitespace including CR")
{
    auto trimmed = [](std::string s) { cp::trim(s); return s; };

    CHECK(trimmed("  value  ") == "value");
    CHECK(trimmed("\tvalue\t") == "value");
    CHECK(trimmed("") == "");
    CHECK(trimmed("   ") == "");
    CHECK(trimmed("a b") == "a b");   // interior whitespace is preserved

    // The reason CR is in the trim set: std::getline strips \n but leaves \r,
    // so every line of a CRLF file arrives with a trailing \r and "true\r"
    // would not compare equal to "true".
    CHECK(trimmed("true\r") == "true");
    CHECK(trimmed("\r\ntrue\r\n") == "true");
}

TEST_CASE("ini lines split into key and value")
{
    std::string key, value;

    SUBCASE("plain assignment")
    {
        REQUIRE(cp::ini_line("use_devtools = true", key, value));
        CHECK(key == "use_devtools");
        CHECK(value == "true");
    }

    SUBCASE("no surrounding spaces")
    {
        REQUIRE(cp::ini_line("debug_port=8888", key, value));
        CHECK(key == "debug_port");
        CHECK(value == "8888");
    }

    SUBCASE("a CRLF line still yields a clean value")
    {
        REQUIRE(cp::ini_line("use_hotkeys = true\r", key, value));
        CHECK(value == "true");
    }

    SUBCASE("an empty value is legal — plugins_dir uses it")
    {
        REQUIRE(cp::ini_line("plugins_dir =", key, value));
        CHECK(key == "plugins_dir");
        CHECK(value == "");
    }

    SUBCASE("only the first = splits; later ones belong to the value")
    {
        REQUIRE(cp::ini_line("plugins_dir = C:\\a=b\\c", key, value));
        CHECK(key == "plugins_dir");
        CHECK(value == "C:\\a=b\\c");
    }
}

TEST_CASE("ini lines that carry no assignment are skipped")
{
    std::string key, value;

    CHECK_FALSE(cp::ini_line("", key, value));
    CHECK_FALSE(cp::ini_line("   ", key, value));
    CHECK_FALSE(cp::ini_line("; a comment", key, value));
    CHECK_FALSE(cp::ini_line("# another comment", key, value));
    CHECK_FALSE(cp::ini_line("   ; indented comment", key, value));
    CHECK_FALSE(cp::ini_line("[client]", key, value));
    CHECK_FALSE(cp::ini_line("  [app]  ", key, value));
    CHECK_FALSE(cp::ini_line("no_equals_here", key, value));
    CHECK_FALSE(cp::ini_line("= orphan value", key, value));
}

TEST_CASE("bool parsing matches the host-side IniReader.ParseBool surface")
{
    // Both sides write this file. If the two parsers disagree on a form, a
    // setting toggled in the hub reads back differently in the core.
    SUBCASE("accepted true forms")
    {
        CHECK(cp::as_bool("1", false));
        CHECK(cp::as_bool("true", false));
        CHECK(cp::as_bool("TRUE", false));
        CHECK(cp::as_bool("True", false));
        CHECK(cp::as_bool("yes", false));
        CHECK(cp::as_bool("YES", false));
    }

    SUBCASE("accepted false forms")
    {
        CHECK_FALSE(cp::as_bool("0", true));
        CHECK_FALSE(cp::as_bool("false", true));
        CHECK_FALSE(cp::as_bool("FALSE", true));
        CHECK_FALSE(cp::as_bool("no", true));
        CHECK_FALSE(cp::as_bool("NO", true));
    }

    SUBCASE("anything else yields the fallback rather than guessing")
    {
        CHECK(cp::as_bool("", true));
        CHECK_FALSE(cp::as_bool("", false));
        CHECK(cp::as_bool("maybe", true));
        CHECK_FALSE(cp::as_bool("maybe", false));
        CHECK(cp::as_bool("2", true));
        CHECK_FALSE(cp::as_bool("truthy", false));
    }
}

TEST_CASE("int parsing falls back instead of throwing")
{
    CHECK(cp::as_int("8888", 0) == 8888);
    CHECK(cp::as_int("0", 5) == 0);
    CHECK(cp::as_int("-1", 5) == -1);

    // std::stoi parses a leading number and ignores the tail.
    CHECK(cp::as_int("123abc", 5) == 123);

    // The reason this is wrapped at all: std::stoi throws on unparseable or
    // out-of-range input, and a malformed config value must not take the core
    // down inside a CEF callback.
    CHECK(cp::as_int("", 7) == 7);
    CHECK(cp::as_int("abc", 7) == 7);
    CHECK(cp::as_int("99999999999999999999", 7) == 7);
}
