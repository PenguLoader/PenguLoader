#include "doctest.h"

#include "browser/path_guard.h"
#include "browser/assets_shims.h"

#include <filesystem>

using assets::is_inside;

// `is_inside` is the sandbox boundary for the plugins scheme handler, the
// `?dir` module and the writable-JSON `$write` binding. A false positive here
// is arbitrary file read/write outside the plugins folder, which is what
// PR #145 (openPluginsFolder path escape) was.
#ifdef _WIN32
#   define ROOT "C:\\data\\.pengu\\plugins"
#   define SEP "\\"
#else
#   define ROOT "/data/.pengu/plugins"
#   define SEP "/"
#endif

TEST_CASE("paths inside the root are accepted")
{
    const std::filesystem::path root{ ROOT };

    CHECK(is_inside(root, std::filesystem::path{ ROOT }));
    CHECK(is_inside(root, std::filesystem::path{ ROOT SEP "a.js" }));
    CHECK(is_inside(root, std::filesystem::path{ ROOT SEP "deep" SEP "nested" SEP "a.js" }));

    // `..` that stays within the root after normalization is fine.
    CHECK(is_inside(root, std::filesystem::path{ ROOT SEP "x" SEP ".." SEP "y.js" }));
}

TEST_CASE("traversal out of the root is rejected")
{
    const std::filesystem::path root{ ROOT };

    SUBCASE("plain parent escape")
    {
        CHECK_FALSE(is_inside(root, std::filesystem::path{ ROOT SEP ".." SEP "datastore" }));
        CHECK_FALSE(is_inside(root, std::filesystem::path{ ROOT SEP "a" SEP ".." SEP ".." SEP "config" }));
    }

    SUBCASE("a sibling whose name merely starts with the root")
    {
        // The separator check exists for exactly this: `<root>-evil` shares a
        // string prefix with `<root>` but is a different directory.
        CHECK_FALSE(is_inside(root, std::filesystem::path{ ROOT "-evil" SEP "a.js" }));
        CHECK_FALSE(is_inside(root, std::filesystem::path{ ROOT "x" }));
    }

    SUBCASE("an unrelated absolute path")
    {
#ifdef _WIN32
        CHECK_FALSE(is_inside(root, std::filesystem::path{ "C:\\Windows\\System32\\drivers\\etc\\hosts" }));
#else
        CHECK_FALSE(is_inside(root, std::filesystem::path{ "/etc/passwd" }));
#endif
    }

    SUBCASE("a prefix of the root is not inside it")
    {
        CHECK_FALSE(is_inside(root, std::filesystem::path{ ROOT }.parent_path()));
    }
}

#ifdef _WIN32
TEST_CASE("Windows comparison is case-insensitive and separator-agnostic")
{
    const std::filesystem::path root{ ROOT };

    // NTFS is case-insensitive by default, so a case-varied path is the same
    // directory and must not be treated as an escape.
    CHECK(is_inside(root, std::filesystem::path{ "C:\\DATA\\.PENGU\\PLUGINS\\a.js" }));

    // make_preferred() normalizes forward slashes, which is what URLs carry.
    CHECK(is_inside(root, std::filesystem::path{ "C:/data/.pengu/plugins/a.js" }));
    CHECK_FALSE(is_inside(root, std::filesystem::path{ "C:/data/.pengu/other/a.js" }));
}
#endif

TEST_CASE("fnv32_1a matches the reference vectors")
{
    // The scheme handler hashes file extensions with this to pick an import
    // shim, and the hub hashes plugin paths with the same function to record
    // which are disabled — so a drift here silently re-enables plugins.
    CHECK(assets::fnv32_1a("", 0) == 2166136261u);
    CHECK(assets::fnv32_1a("a", 1) == 0xe40c292cu);
    CHECK(assets::fnv32_1a("foobar", 6) == 0xbf9cf968u);

    // The compile-time UDL and the runtime call must agree, since extensions
    // are hashed at compile time into KNOWN_ASSETS_SET and at runtime from a
    // std::u16string.
    static_assert("png"_hash == assets::fnv32_1a("png", 3), "UDL/runtime drift");

    const char16_t wide[] = u"png";
    CHECK(assets::fnv32_1a(wide, 3) == "png"_hash);
}

TEST_CASE("known asset extensions resolve through the hash set")
{
    CHECK(assets::KNOWN_ASSETS_SET.count("png"_hash) == 1);
    CHECK(assets::KNOWN_ASSETS_SET.count("svg"_hash) == 1);
    CHECK(assets::KNOWN_ASSETS_SET.count("nope"_hash) == 0);
}
