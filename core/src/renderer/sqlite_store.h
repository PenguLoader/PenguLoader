#pragma once

// Shared SQLite setup for the renderer's two stores — window.DataStore and the
// per-plugin context.storage.
//
// They keep different schemas and different quotas, but the *tuning* is
// identical and several parts of it are easy to get subtly wrong in ways that
// do not fail loudly: a WAL that never engaged, an auto_vacuum that never took,
// a vacuum that reclaims one page. Getting those right twice, independently, is
// how they drift.
//
// Rationale for the individual pragmas is in docs/plugin-storage.md sections 9
// and 11.

#include "pengu.h"

#include "sqlite3.h"

#include <cstring>
#include <string>

namespace sqlite_store
{
    /// First column of the first row of `sql`, as an int. 0 if it produced
    /// nothing — every caller here reads a pragma that always returns a row.
    inline int pragma_int(sqlite3 *db, const char *sql)
    {
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK)
            return 0;

        int value = 0;
        if (sqlite3_step(st) == SQLITE_ROW)
            value = sqlite3_column_int(st, 0);
        sqlite3_finalize(st);
        return value;
    }

    ///
    /// Open (creating if needed) and tune a store.
    ///
    /// Returns null on failure, which every caller treats as "this store is
    /// unavailable" rather than an error to report — there is no logging
    /// facility in core, and both stores' write paths are already
    /// fire-and-forget from the renderer's point of view.
    ///
    /// The caller creates its own schema afterwards.
    ///
    /// @param file Database path. Its parent must already exist.
    /// @param quota_bytes Hard ceiling, enforced by SQLite itself. 0 for none.
    ///
    inline sqlite3 *open_tuned(const path &file, uint64_t quota_bytes)
    {
        auto utf8_path = file.u8string();
        std::string utf8(utf8_path.begin(), utf8_path.end());

        sqlite3 *db = nullptr;
        if (sqlite3_open_v2(utf8.c_str(), &db,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
        {
            if (db) sqlite3_close(db);
            return nullptr;
        }

        // Must precede the first write, or it is ignored entirely.
        sqlite3_exec(db, "PRAGMA page_size=8192;", nullptr, nullptr, nullptr);

        // Incremental auto-vacuum, so deleting rows actually returns space
        // rather than leaving the file at its high-water mark forever.
        //
        // This can only be turned on before the first table exists, or by
        // rewriting the whole file. A store created before this landed reads
        // back 0 and takes the VACUUM once; a fresh one does not. Eager rather
        // than lazy because the alternative is a store that silently never
        // reclaims, which is indistinguishable from a quota bug.
        sqlite3_exec(db, "PRAGMA auto_vacuum=INCREMENTAL;", nullptr, nullptr, nullptr);
        if (pragma_int(db, "PRAGMA auto_vacuum;") != 2)
            sqlite3_exec(db, "VACUUM;", nullptr, nullptr, nullptr);

        // WAL can silently fail to engage: it needs shared memory, which
        // network filesystems do not provide, and the data root can be a
        // redirected UNC path on a roaming profile. The pragma's *return
        // value* is the only reliable signal — not the absence of an error.
        bool wal = false;
        sqlite3_stmt *st = nullptr;
        if (sqlite3_prepare_v2(db, "PRAGMA journal_mode=WAL;", -1, &st, nullptr) == SQLITE_OK)
        {
            if (sqlite3_step(st) == SQLITE_ROW)
            {
                auto *mode = reinterpret_cast<const char *>(sqlite3_column_text(st, 0));
                wal = mode != nullptr && std::strcmp(mode, "wal") == 0;
            }
            sqlite3_finalize(st);
        }
        if (!wal)
            sqlite3_exec(db, "PRAGMA journal_mode=TRUNCATE;", nullptr, nullptr, nullptr);

        sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA busy_timeout=3000;", nullptr, nullptr, nullptr);

        // High, because checkpointing on every transaction costs ~3.4x on
        // writes. The price is a log that grows without bound, which is what
        // `maintain` below exists to pay.
        sqlite3_exec(db, "PRAGMA wal_autocheckpoint=20000;", nullptr, nullptr, nullptr);

        // Last, and computed from the *actual* page size rather than the one
        // requested above: an existing store may have been created with a
        // different one, and a cap derived from the wrong page size is the
        // wrong cap. After the VACUUM, which would otherwise hit the ceiling
        // it is trying to make room under.
        //
        // Note this is a connection setting, not a property of the file, so it
        // has to be re-applied on every open and it bounds growth rather than
        // shrinking a store that somehow arrived oversized.
        if (quota_bytes > 0)
        {
            const int page_size = pragma_int(db, "PRAGMA page_size;");
            if (page_size > 0)
            {
                const auto max_pages = static_cast<int>(quota_bytes / page_size);
                sqlite3_exec(db, ("PRAGMA max_page_count=" + std::to_string(max_pages) + ";").c_str(),
                             nullptr, nullptr, nullptr);
            }
        }

        return db;
    }

    ///
    /// Idle maintenance: fold the write-ahead log back into the database and
    /// return freed pages to the filesystem.
    ///
    /// Call when the owning thread's queue is empty, never on the write path.
    ///
    /// TRUNCATE rather than PASSIVE because passive leaves the log at its
    /// high-water mark, which is the thing being fixed — measured at 64 MB
    /// against an 8 KB database after one burst.
    ///
    /// sqlite3_exec rather than prepare-and-step for the vacuum: it does its
    /// work one page per sqlite3_step, so stepping once reclaims exactly one
    /// page and looks like it did nothing. exec steps to completion.
    ///
    inline void maintain(sqlite3 *db)
    {
        if (db == nullptr)
            return;

        sqlite3_wal_checkpoint_v2(db, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr, nullptr);
        sqlite3_exec(db, "PRAGMA incremental_vacuum;", nullptr, nullptr, nullptr);
    }

    /// Bytes actually occupied — not the file size, which SQLite does not
    /// shrink on delete. This is what a `max_page_count` cap is measured
    /// against.
    inline double used_bytes(sqlite3 *db)
    {
        const int pages = pragma_int(db, "PRAGMA page_count;");
        const int free_pages = pragma_int(db, "PRAGMA freelist_count;");
        const int page_size = pragma_int(db, "PRAGMA page_size;");
        const int used = pages > free_pages ? pages - free_pages : 0;
        return static_cast<double>(used) * page_size;
    }
}
