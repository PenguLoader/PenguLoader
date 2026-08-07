# SQLite — vendored amalgamation

**Version 3.53.4** (`SQLITE_VERSION_NUMBER` 3053004)

| | |
| --- | --- |
| Source | <https://sqlite.org/2026/sqlite-amalgamation-3530400.zip> |
| Archive SHA3-256 | `628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e` |
| Archive size | 2,946,650 bytes |
| Retrieved | 2026-08-07 |

The hash above is the one published on <https://sqlite.org/download.html>, and it
was verified against the downloaded archive before these files were committed.
Re-verify it on any bump — SQLite publishes SHA3-256, not SHA-256, so
`Get-FileHash` cannot check it; use `python -c "import hashlib; ..."` or
`openssl dgst -sha3-256`.

## Files

| file | kept | why |
| --- | :-: | --- |
| `sqlite3.c` | ✅ | the library, one translation unit |
| `sqlite3.h` | ✅ | public API |
| `sqlite3ext.h` | ✅ | referenced by `sqlite3.c`; tiny, and omitting it only invites a confusing build break |
| `shell.c` | ❌ | the `sqlite3` command-line tool — not a library, never built here |

**Unmodified.** Nothing in this directory is patched. Every build-time choice is
expressed as a compile definition in `core/CMakeLists.txt` (and the macOS
`core/Makefile`), never as an edit here — so a version bump is a clean file
replacement rather than a merge.

## Build flags

Rationale in [`docs/plugin-storage.md` §9.3](../../../docs/plugin-storage.md).

| define | why |
| --- | --- |
| `SQLITE_OMIT_LOAD_EXTENSION` | **security.** Without it, a path that reaches SQL could load a DLL. Plugins never get SQL, so this is defence in depth for a boundary that should never be tested. |
| `SQLITE_DQS=0` | double-quoted string literals are a misfeature; off means a typo'd identifier errors instead of silently becoming a string |
| `SQLITE_THREADSAFE=1` | serialized. The store is owned by one dedicated thread, but the renderer is not a single-threaded process and the cost is a mutex we would otherwise hand-roll |
| `SQLITE_OMIT_DEPRECATED` | dead API surface |
| `SQLITE_DEFAULT_MEMSTATUS=0` | drops per-allocation bookkeeping we never read |
| `SQLITE_OMIT_UTF16` | the API takes UTF-8; UTF-16 conversion happens at the V8 boundary via `CefStrBase::to_utf8_into` |
| `SQLITE_LIKE_DOESNT_MATCH_BLOBS` | size, and no behaviour we depend on |

## Bumping

1. Download the new amalgamation and **verify the published SHA3-256**.
2. Replace `sqlite3.c`, `sqlite3.h`, `sqlite3ext.h`. Do not merge — replace.
3. Update the version, URL, hash, size and date in this file.
4. Rebuild and run `ctest --test-dir core/build`.
5. Deploy to a live client — a version bump changes `core.dll`, which must be
   re-signed before `boot.dll` will inject it.

## Why vendored rather than a package manager

macOS builds `core.dylib` through `core/Makefile`, not CMake, and no package
manager is integrated with it. The amalgamation is one C file both build systems
already know how to compile, the exact bytes are pinned by the repository, and
the flags above need no overlay port. See the discussion in
[`docs/plugin-storage.md` §3](../../../docs/plugin-storage.md).
