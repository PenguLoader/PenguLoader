cmake_minimum_required(VERSION 3.21)

# Native half of Pengu.
#
#   core.dll — injected into LeagueClientUx, hooks libcef.
#   boot.dll — the IFEO entry point under rundll32; verifies core's embedded
#              signature, then injects it.
#
# They share a directory because boot compiles `src/bootstrap.cc` out of core
# and nothing else. They are peers, not parent and child: boot embeds the
# public key that authorises core, and per docs/windows-activation.md the boot
# is frozen while core evolves. New capability belongs in core.
#
# Windows only, deliberately. macOS still builds core.dylib through
# `make -C core release` — that link line (weak `-lcef`, `-flat_namespace`, the
# insert_dylib post-processing step) has not been ported here, and porting it
# blind on a machine that cannot run it would be worse than leaving it alone.
#
#   cmake -S core -B core/build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build core/build

project(pengu-native LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR
        "core/CMakeLists.txt builds the Windows targets only. "
        "On macOS use: make -C core release")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Version ─────────────────────────────────────────────────────────────────
# Single-sourced from the root package.json, the same way app/Directory.Build.props
# does for the .NET side. Without this the .rc drifts, which is how core ended up
# claiming to be D3D9 10.0.19041.2311.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../package.json" _pengu_package_json)
string(JSON PENGU_VERSION GET "${_pengu_package_json}" version)
if(NOT PENGU_VERSION)
    set(PENGU_VERSION "0.0.0")
endif()

# FILEVERSION needs comma-separated numerics, so drop any prerelease tag.
string(REGEX REPLACE "-.*$" "" _pengu_version_numeric "${PENGU_VERSION}")
string(REPLACE "." "," _pengu_version_commas "${_pengu_version_numeric}")
set(PENGU_VERSION_COMMAS "${_pengu_version_commas},0")

message(STATUS "Pengu version: ${PENGU_VERSION} (${PENGU_VERSION_COMMAS})")

# ── Shared settings ─────────────────────────────────────────────────────────
# Both DLLs link the CRT statically. For boot that is load-bearing: it runs on
# every client launch and a missing VC redistributable would brick the client in
# exactly the way the boot design exists to prevent.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Preserve the layout the rest of the repo already expects — the app csproj
# stages from here, CI copies from here, and %LOCALAPPDATA%\.pengu\active points
# at a directory in this shape. Embedding $<CONFIG> stops CMake appending a
# second per-config directory under multi-config generators.
set(PENGU_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/x64/$<CONFIG>")

# `Release` here means "shipping build with a PDB", matching what the vcxproj
# did via GenerateDebugInformation on Release.
add_compile_options($<$<CONFIG:Release>:/Zi>)
add_link_options($<$<CONFIG:Release>:/DEBUG>)

# /FC makes __FILE__ an absolute path. renderer.cc's debug build resolves the
# preload script against __FILE__ so it finds the source tree it was built
# from, wherever the DLL is deployed to — a relative __FILE__ would resolve
# against the client's working directory instead. Ninja already passes absolute
# source paths, but this makes it independent of the generator.
add_compile_options($<$<CONFIG:Debug>:/FC>)

# CMake's stock Release flags are `/O2 /Ob2 /DNDEBUG`. Optimisation is set
# per-target below — core favours speed, boot favours size — so strip the
# defaults instead of appending and collecting D9025 override warnings.
set(CMAKE_CXX_FLAGS_RELEASE "/DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "/DNDEBUG")

# ── sqlite3 ─────────────────────────────────────────────────────────────────
# The vendored amalgamation (core/vendor/sqlite), built as its own static
# library rather than folded into core's source list:
#
#   - the SQLITE_* configuration cannot leak into core's own translation units,
#   - warnings from a quarter-million lines of third-party C stay out of core's
#     build log without having to lower core's own warning level,
#   - and it rebuilds only when the vendored source is bumped, which is roughly
#     never, rather than on every core edit.
add_library(sqlite3 STATIC vendor/sqlite/sqlite3.c)

target_include_directories(sqlite3 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sqlite")

# PUBLIC, not PRIVATE. sqlite3.h itself branches on these — SQLITE_OMIT_DEPRECATED
# gates a block of prototypes, SQLITE_OMIT_LOAD_EXTENSION another — so a consumer
# compiling the header without them would see an API the library does not
# actually contain, and find out at link time or later.
#
# Per-flag rationale is in core/vendor/sqlite/README.md. The one that is not
# about size or speed is SQLITE_OMIT_LOAD_EXTENSION: without it, any path that
# reached SQL could load a DLL.
target_compile_definitions(sqlite3 PUBLIC
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_DQS=0
    SQLITE_THREADSAFE=1
    SQLITE_DEFAULT_MEMSTATUS=0
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    SQLITE_LIKE_DOESNT_MATCH_BLOBS
    SQLITE_MAX_EXPR_DEPTH=0
    SQLITE_OMIT_DECLTYPE
    SQLITE_OMIT_DEPRECATED
    SQLITE_OMIT_PROGRESS_CALLBACK
    SQLITE_OMIT_SHARED_CACHE
    SQLITE_USE_ALLOCA
)

# /w rather than core's /W3: this is third-party source we do not edit, so its
# warnings are noise we cannot act on. No LTO either — one 9.5 MB translation
# unit gains nothing from cross-module inlining and costs minutes of link time.
target_compile_options(sqlite3 PRIVATE /w $<$<CONFIG:Release>:/O2>)
set_property(TARGET sqlite3 PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)

set_target_properties(sqlite3 PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${PENGU_OUTPUT_DIR}"
)

# ── core.dll ────────────────────────────────────────────────────────────────
# Listed explicitly rather than globbed: a glob does not re-run configure when a
# file is added, so a new source silently goes unbuilt until someone reconfigures.
add_library(core SHARED
    src/browser/assets.cc
    src/browser/browser.cc
    src/browser/devtools.cc
    src/browser/keyboard.cc
    src/browser/riotclient.cc
    src/browser/window.cc
    src/bootstrap.cc
    src/config.cc
    src/libcef.cc
    src/dllproxy.cc
    src/dllmain.cc
    src/signature.cc
    src/renderer/v8_datastore.cc
    src/renderer/v8_dir.cc
    src/renderer/v8_helper.cc
    src/renderer/v8_json_write.cc
    src/renderer/v8_pluginfs.cc
    src/renderer/v8_storage.cc
    src/renderer/renderer.cc
    src/utils/cefstr.cc
    src/utils/dylib.cc
    src/utils/file.cc
    src/utils/shell.cc
    src/utils/window.cc
    res/resource.rc
)

set_target_properties(core PROPERTIES
    OUTPUT_NAME "core"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${PENGU_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${PENGU_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${PENGU_OUTPUT_DIR}"
)

target_include_directories(core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/cef"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(core PRIVATE
    UNICODE _UNICODE
    CORE_EXPORTS _WINDOWS _USRDLL
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

# /permissive is required, not cosmetic. The vcxproj set ConformanceMode=false,
# and `/std:c++20` alone implies `/permissive-`, under which the `&CefStr(...)`
# and `&u"..."_s` idioms used throughout the CEF C-API call sites fail with
# C2102 ("'&' requires l-value") — taking the address of a temporary is an MSVC
# extension this code leans on heavily.
#
# /Ob1 rather than the /Ob2 implied by /O2 — carried over from the vcxproj,
# which set InlineFunctionExpansion=OnlyExplicitInline on Release.
target_compile_options(core PRIVATE
    /W3 /permissive
    $<$<CONFIG:Release>:/O2 /Ob1 /Oi /Ot /Gy>
)

# libcef is delay-loaded so core can be injected before CEF is resident; the
# hooks in libcef.cc resolve it by signature scan at runtime.
target_link_libraries(core PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/cef/lib/win/libcef.lib"
    delayimp
    sqlite3
)

target_link_options(core PRIVATE
    /SUBSYSTEM:WINDOWS
    /DELAYLOAD:libcef.dll
    "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/res/module.def"
    $<$<CONFIG:Release>:/OPT:REF>
    $<$<CONFIG:Release>:/OPT:ICF>
)

set_property(TARGET core PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

set_source_files_properties(res/resource.rc PROPERTIES
    COMPILE_DEFINITIONS "PENGU_VERSION_COMMAS=${PENGU_VERSION_COMMAS};PENGU_VERSION_STR=\"${PENGU_VERSION}\""
)

# renderer.cc #includes ../../packages/preload/dist/preload.g.h, produced by
# `pnpm --filter @pengujs/preload build`. Generating it is not modelled here —
# the generator lives in the JS workspace, and CI builds it before this.
#
# Depending on it is modelled, though. Ninja scans #includes only for paths it
# can resolve at configure time, and a rebuilt preload otherwise leaves
# renderer.cc's object untouched: `ninja` reports "no work to do" and you ship a
# core.dll carrying the previous bundle. That failure is silent and costs a
# whole deploy cycle to notice.
set_source_files_properties(src/renderer/renderer.cc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../packages/preload/dist/preload.g.h"
)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../packages/preload/dist/preload.g.h")
    message(WARNING
        "packages/preload/dist/preload.g.h is missing — core will fail to compile. "
        "Run: pnpm --filter @pengujs/preload build")
endif()

add_subdirectory(boot)

# Unit tests for the CEF-free logic under src/. On by default because they
# cost a couple of seconds; -DPENGU_BUILD_TESTS=OFF skips them.
option(PENGU_BUILD_TESTS "Build the core unit tests" ON)
if(PENGU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
