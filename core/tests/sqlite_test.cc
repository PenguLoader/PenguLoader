#include "doctest.h"

#include "sqlite3.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

// The vendored SQLite, checked against the configuration docs/plugin-storage.md
// depends on. Linking is not the interesting part — these assert that the
// compile definitions in core/CMakeLists.txt actually took effect, which a
// build log cannot tell you and a bump could silently undo.
//
// No CEF here, so this still honours the tier's one rule.

namespace {

/// Open an in-memory database, or fail the test loudly rather than
/// null-dereferencing through the rest of the case.
struct Db
{
    sqlite3 *h = nullptr;

    explicit Db(const char *path = ":memory:")
    {
        REQUIRE(sqlite3_open(path, &h) == SQLITE_OK);
        REQUIRE(h != nullptr);
    }

    ~Db() { sqlite3_close(h); }

    int exec(const char *sql) { return sqlite3_exec(h, sql, nullptr, nullptr, nullptr); }

    /// First column of the first row, as text.
    std::string scalar(const char *sql)
    {
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(h, sql, -1, &st, nullptr) != SQLITE_OK)
            return "<prepare failed>";

        std::string out;
        if (sqlite3_step(st) == SQLITE_ROW)
        {
            auto *t = reinterpret_cast<const char *>(sqlite3_column_text(st, 0));
            if (t) out = t;
        }
        sqlite3_finalize(st);
        return out;
    }
};

}

TEST_CASE("the vendored version is the one the README documents")
{
    // If this fails, core/vendor/sqlite/README.md is lying about what is in the
    // tree — which matters, because that file is the provenance record.
    CHECK(std::string(sqlite3_libversion()) == "3.53.4");
    CHECK(sqlite3_libversion_number() == 3053004);
}

TEST_CASE("a WITHOUT ROWID key/value table round-trips")
{
    Db db;

    // The schema from docs/plugin-storage.md section 4.
    REQUIRE(db.exec(
        "CREATE TABLE kv ("
        "  k TEXT PRIMARY KEY,"
        "  fmt INTEGER NOT NULL,"
        "  v BLOB NOT NULL,"
        "  mtime INTEGER NOT NULL"
        ") WITHOUT ROWID;") == SQLITE_OK);

    sqlite3_stmt *st = nullptr;
    REQUIRE(sqlite3_prepare_v2(db.h,
        "INSERT INTO kv(k,fmt,v,mtime) VALUES(?,?,?,?)"
        " ON CONFLICT(k) DO UPDATE SET fmt=excluded.fmt, v=excluded.v, mtime=excluded.mtime",
        -1, &st, nullptr) == SQLITE_OK);

    // Values are opaque bytes, so NULs inside them must survive -- section 6.2
    // stores JSON text and raw buffers through the same column.
    const std::string value("{\"a\":1}\0trailing", 16);

    sqlite3_bind_text(st, 1, "profile", -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 2, 0);
    sqlite3_bind_blob(st, 3, value.data(), static_cast<int>(value.size()), SQLITE_STATIC);
    sqlite3_bind_int64(st, 4, 1754530000000LL);
    CHECK(sqlite3_step(st) == SQLITE_DONE);
    sqlite3_finalize(st);

    SUBCASE("the value comes back byte-for-byte")
    {
        REQUIRE(sqlite3_prepare_v2(db.h, "SELECT v, fmt, mtime FROM kv WHERE k='profile'",
                                   -1, &st, nullptr) == SQLITE_OK);
        REQUIRE(sqlite3_step(st) == SQLITE_ROW);

        const int n = sqlite3_column_bytes(st, 0);
        const auto *p = static_cast<const unsigned char *>(sqlite3_column_blob(st, 0));
        const std::string got(reinterpret_cast<const char *>(p), n);

        CHECK(n == 16);
        CHECK(got == value);
        CHECK(sqlite3_column_int(st, 1) == 0);
        CHECK(sqlite3_column_int64(st, 2) == 1754530000000LL);
        sqlite3_finalize(st);
    }

    SUBCASE("upsert replaces rather than duplicating")
    {
        REQUIRE(db.exec(
            "INSERT INTO kv(k,fmt,v,mtime) VALUES('profile',1,x'FF00FF',2)"
            " ON CONFLICT(k) DO UPDATE SET fmt=excluded.fmt, v=excluded.v,"
            " mtime=excluded.mtime;") == SQLITE_OK);

        CHECK(db.scalar("SELECT COUNT(*) FROM kv") == "1");
        CHECK(db.scalar("SELECT fmt FROM kv WHERE k='profile'") == "1");
        CHECK(db.scalar("SELECT length(v) FROM kv WHERE k='profile'") == "3");
    }
}

TEST_CASE("SQLITE_OMIT_LOAD_EXTENSION is in effect")
{
    // The one flag that is about security rather than size. If a future bump
    // drops it, a path that reached SQL could load a DLL.
#ifndef SQLITE_OMIT_LOAD_EXTENSION
    FAIL("SQLITE_OMIT_LOAD_EXTENSION is not defined for consumers of sqlite3.h");
#endif

    Db db;
    // The SQL function is what an injected statement would reach for, and it
    // must not exist. (The C API sqlite3_load_extension is compiled out
    // entirely, so it cannot even be named here.)
    CHECK(db.exec("SELECT load_extension('foo');") != SQLITE_OK);
    CHECK(std::string(sqlite3_errmsg(db.h)).find("load_extension") != std::string::npos);
}

TEST_CASE("SQLITE_DQS=0 makes a double-quoted string an identifier error")
{
    Db db;
    REQUIRE(db.exec("CREATE TABLE t(a TEXT);") == SQLITE_OK);

    // With DQS on, this silently becomes the *string* 'b' and inserts a row.
    // With DQS=0 it is a misspelled column name, which is what we want: a typo
    // should fail rather than quietly store the wrong thing.
    CHECK(db.exec("INSERT INTO t(a) VALUES(\"b\");") != SQLITE_OK);

    // Single quotes are the real string syntax and must still work.
    CHECK(db.exec("INSERT INTO t(a) VALUES('b');") == SQLITE_OK);
    CHECK(db.scalar("SELECT a FROM t") == "b");
}

TEST_CASE("threading mode is serialized")
{
    // docs/plugin-storage.md section 10 pins the store to one dedicated
    // thread, but the renderer is not single-threaded and SQLITE_THREADSAFE=1
    // is what makes a stray cross-thread call safe rather than corrupting.
    CHECK(sqlite3_threadsafe() != 0);
}

TEST_CASE("WAL engages on a real file, and reports what it engaged")
{
    // Section 9.2: WAL needs shared memory and silently does not engage on a
    // network filesystem, so the pragma's *return value* is the thing to trust.
    // This asserts the check works; it cannot exercise the UNC case here.
    const auto path = std::filesystem::temp_directory_path() / "pengu-sqlite-wal-check.db";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    {
        Db db(path.string().c_str());
        const auto mode = db.scalar("PRAGMA journal_mode=WAL;");
        CHECK(mode == "wal");

        CHECK(db.exec("CREATE TABLE t(a);") == SQLITE_OK);
        CHECK(db.exec("INSERT INTO t VALUES(1);") == SQLITE_OK);

        // The -wal sibling should exist while the connection is open.
        CHECK(std::filesystem::exists(path.string() + "-wal", ec));
    }

    // ...and the data survives reopening.
    {
        Db db(path.string().c_str());
        CHECK(db.scalar("SELECT a FROM t") == "1");
    }

    std::filesystem::remove(path, ec);
    std::filesystem::remove(path.string() + "-wal", ec);
    std::filesystem::remove(path.string() + "-shm", ec);
}
