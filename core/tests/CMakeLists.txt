# Fast unit tests for the pure logic in core/src.
#
# The one rule: these link nothing from CEF. If a test needs cef_request_t or a
# libcef export, it is the wrong test — that behaviour belongs in the live CDP
# harness under packages/preload/scripts/, driven against a running client.
#
# That split is not fastidiousness. The bug that shipped in the range rewrite
# was CEF calling skip() before get_response_headers(); no unit test would have
# caught it, and only an A/B against a real client did. These tests exist to
# make the pure logic unbreakable so the slow tier only has to hunt integration
# bugs.
#
#   ctest --test-dir core/build --output-on-failure
#   ./core/bin/x64/Release/pengu-tests            # direct, for doctest filters

add_executable(pengu-tests
    main.cc
    range_test.cc
    path_test.cc
    config_test.cc
    utf_test.cc
    sqlite_test.cc
)

# The vendored database is not CEF, so it does not break the rule above. It is
# linked because sqlite_test.cc asserts that the compile definitions actually
# took effect — something no build log will tell you, and something a version
# bump could silently undo.
target_link_libraries(pengu-tests PRIVATE sqlite3)

target_include_directories(pengu-tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../vendor/doctest"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

# No CEF include path on purpose — an accidental CEF dependency should fail to
# compile here rather than quietly making the suite unbuildable off-Windows.

set_target_properties(pengu-tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PENGU_OUTPUT_DIR}"
)

if(MSVC)
    # Same permissive mode as core: the headers under test are shared with
    # translation units that rely on MSVC's address-of-temporary extension.
    target_compile_options(pengu-tests PRIVATE /W3 /permissive /EHsc)
endif()

add_test(NAME pengu-tests COMMAND pengu-tests)
