#include "doctest.h"

#include "sha256.h"
#include "storage_key.h"

#include <algorithm>
#include <string>

// SHA-256 against the published FIPS 180-4 vectors, and the plugin -> database
// filename derivation built on it.
//
// The vectors are the point. A hand-written hash that is subtly wrong still
// produces stable-looking hex, so "it returns 32 characters" proves nothing —
// only agreement with the standard does. And this one names storage files, so
// being wrong means either colliding stores or, on a later fix, every plugin
// silently losing its data.

static std::string hex(const std::string &s)
{
    return sha256::hex(s.data(), s.size());
}

TEST_CASE("FIPS 180-4 vectors")
{
    SUBCASE("empty input")
        CHECK(hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    SUBCASE("abc — one block, short")
        CHECK(hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    SUBCASE("448 bits — padding fills the same block")
        CHECK(hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
              == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    SUBCASE("896 bits — padding spills into a second block")
        CHECK(hex("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
                  "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu")
              == "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
}

TEST_CASE("block-boundary lengths")
{
    // 55, 56, 63, 64 and 65 bytes are where a padding implementation goes
    // wrong: 56 is the first length whose trailer no longer fits, and 64 is a
    // whole block with nothing left over.
    const std::string a(55, 'a');
    const std::string b(56, 'a');
    const std::string c(63, 'a');
    const std::string d(64, 'a');
    const std::string e(65, 'a');

    CHECK(hex(a) == "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    CHECK(hex(b) == "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
    CHECK(hex(c) == "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34");
    CHECK(hex(d) == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    CHECK(hex(e) == "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0");
}

TEST_CASE("streaming in pieces matches one shot")
{
    const std::string message =
        "the quick brown fox jumps over the lazy dog, repeatedly, at length, "
        "so that this crosses more than one 64-byte compression block boundary";

    const std::string once = hex(message);

    for (size_t chunk : { size_t(1), size_t(7), size_t(63), size_t(64), size_t(65) })
    {
        sha256::Hasher hasher;
        for (size_t i = 0; i < message.size(); i += chunk)
            hasher.update(message.data() + i, std::min(chunk, message.size() - i));

        uint8_t digest[sha256::DIGEST_BYTES];
        hasher.finish(digest);

        std::string got;
        static const char *digits = "0123456789abcdef";
        for (auto byte : digest)
        {
            got.push_back(digits[byte >> 4]);
            got.push_back(digits[byte & 0x0F]);
        }

        CHECK(got == once);
    }
}

TEST_CASE("hex truncation takes a prefix of the full digest")
{
    const std::string full = hex("abc");
    CHECK(sha256::hex("abc", 3, 16) == full.substr(0, 32));
    CHECK(sha256::hex("abc", 3, 16).size() == 32);

    // Asking for more than exists is clamped rather than reading past the end.
    CHECK(sha256::hex("abc", 3, 999) == full);
}

// ---------------------------------------------------------------------------
// storage_key
// ---------------------------------------------------------------------------

TEST_CASE("canonical form folds the things a filesystem folds")
{
    using storage::canonical;

    CHECK(canonical("@nomi-san/snooze-css/index.js") == "@nomi-san/snooze-css/index.js");

    SUBCASE("backslashes — renderer.cc builds entries with path joins")
        CHECK(canonical("@nomi-san\\snooze-css\\index.js") == "@nomi-san/snooze-css/index.js");

    SUBCASE("case — the same plugin can present either way on Windows/macOS")
        CHECK(canonical("@Nomi-San/Snooze-CSS/Index.js") == "@nomi-san/snooze-css/index.js");

    SUBCASE("the v1.1.6 rename-to-disable suffix")
        CHECK(canonical("indie-theme.js_") == "indie-theme.js");

    SUBCASE("only one trailing underscore, and only at the end")
        CHECK(canonical("a_b.js") == "a_b.js");
}

TEST_CASE("a database name is 32 hex characters plus .db")
{
    const std::string name = storage::db_name("@nomi-san/snooze-css/index.js");

    CHECK(name.size() == 32 + 3);
    CHECK(name.substr(32) == ".db");
    CHECK(name.find_first_not_of("0123456789abcdef") == 32);
}

TEST_CASE("the name is stable, and is the documented derivation")
{
    // Literals, not a re-derivation through sha256::hex — comparing the
    // function against itself would only prove it calls what we think it
    // calls. Pinned so a refactor of canonical() or of the truncation cannot
    // silently repoint every installed plugin at a fresh, empty database.
    //
    // Independently reproducible:
    //   python -c "import hashlib; print(hashlib.sha256(
    //       b'@nomi-san/snooze-css/index.js').hexdigest()[:32])"
    CHECK(storage::db_stem("@nomi-san/snooze-css/index.js")
          == "e098c5a42ba9d8446d430207cfdff336");

    CHECK(storage::db_stem("indie-theme.js") == "ed53fa69ed943a6cc74f9876d7423955");

    CHECK(storage::db_stem("@nomi-san/snooze-css/index.js")
          == storage::db_stem("@Nomi-San\\Snooze-CSS\\index.js"));
}

TEST_CASE("different plugins get different databases")
{
    // The fork case docs/plugin-install.md exists to handle: same repo name,
    // different owner, must not share a store.
    CHECK(storage::db_stem("@nomi-san/snooze-css/index.js")
          != storage::db_stem("@reformeddoge/snooze-css/index.js"));

    CHECK(storage::db_stem("indie-theme.js") != storage::db_stem("indie-theme2.js"));
    CHECK(storage::db_stem("a/index.js") != storage::db_stem("b/index.js"));
}
