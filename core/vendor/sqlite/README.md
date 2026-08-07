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

Set in **two places that must stay in step** — the `sqlite3` target in
`core/CMakeLists.txt` and `SQLITE_DEFS` in `core/Makefile`. They are `PUBLIC`
in CMake and applied to `CXXFLAGS` in the Makefile, deliberately: `sqlite3.h`
itself branches on `SQLITE_OMIT_DEPRECATED` and `SQLITE_OMIT_LOAD_EXTENSION`,
so a translation unit that included the header without them would see an API
the library does not contain.

Everything except the first row is SQLite's own
[recommended compile-time options](https://sqlite.org/compile.html).
Rationale in [`docs/plugin-storage.md` §9.3](../../../docs/plugin-storage.md).

| define | why |
| --- | --- |
| `SQLITE_OMIT_LOAD_EXTENSION` | **security.** Without it, a path that reaches SQL could load a DLL. Plugins never get SQL, so this is defence in depth for a boundary that should never be tested. `core/tests/sqlite_test.cc` asserts it took effect. |
| `SQLITE_DQS=0` | double-quoted string literals are a misfeature; off means a typo'd identifier errors instead of silently becoming a string |
| `SQLITE_THREADSAFE=1` | serialized. The store is owned by one dedicated thread, but the renderer is not a single-threaded process and the cost is a mutex we would otherwise hand-roll |
| `SQLITE_DEFAULT_MEMSTATUS=0` | drops per-allocation bookkeeping we never read |
| `SQLITE_DEFAULT_WAL_SYNCHRONOUS=1` | matches the `synchronous=NORMAL` the store sets anyway |
| `SQLITE_LIKE_DOESNT_MATCH_BLOBS` | size, and no behaviour we depend on |
| `SQLITE_MAX_EXPR_DEPTH=0` | removes the recursion limiter; we generate our own SQL, so there is no hostile expression to bound |
| `SQLITE_OMIT_DECLTYPE` | `sqlite3_column_decltype` is unused |
| `SQLITE_OMIT_DEPRECATED` | dead API surface |
| `SQLITE_OMIT_PROGRESS_CALLBACK` | unused |
| `SQLITE_OMIT_SHARED_CACHE` | unused, and discouraged upstream |
| `SQLITE_USE_ALLOCA` | stack instead of heap for scratch buffers |

**Not set**, and worth recording so nobody re-adds them casually:

- `SQLITE_OMIT_AUTOINIT` — would require calling `sqlite3_initialize()` by hand
  before first use. A real footgun for a small size win.
- `SQLITE_OMIT_UTF16` — the `SQLITE_OMIT_*` family is only fully supported when
  building from canonical sources, not the amalgamation. This one is not needed
  (the API is used as UTF-8 throughout) and is not worth the risk of a subtle
  runtime difference we could not easily detect.

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
