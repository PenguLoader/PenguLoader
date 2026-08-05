# boot.dll — the stable IFEO entry point, loaded by rundll32.
#
# Deliberately tiny: it links bootstrap.cc out of ../src and nothing else, so
# there is no CEF surface and no shared-CRT dependency. It runs on every client
# launch and stays resident for the whole session, and replacing it costs an
# admin prompt, so it should almost never need to change. New capability
# belongs in core.dll.
#
# A DLL and not an executable because Defender quarantines the executable:
# registering an unknown binary as an IFEO Debugger is scored behaviourally when
# the client triggers it, and code signing does not exempt it. rundll32 is
# Microsoft-signed and is what this project shipped for years.

add_library(boot SHARED
    main.cc
    trust.cc
    ../src/bootstrap.cc
    res/boot.rc
)

set_target_properties(boot PROPERTIES
    OUTPUT_NAME "boot"
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${PENGU_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${PENGU_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${PENGU_OUTPUT_DIR}"
)

target_include_directories(boot PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)

target_compile_definitions(boot PRIVATE
    WIN32 _WINDOWS
    UNICODE _UNICODE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

# Optimised for size, not speed: this is a launcher shim that blocks on the
# client for the whole session, so its resident footprint matters more than its
# throughput.
target_compile_options(boot PRIVATE
    /W3 /sdl
    $<$<CONFIG:Release>:/O1 /Oi /Gy>
)

# bcrypt  — ECDSA P-256 verification of core's embedded signature
# wintrust — Authenticode chain check
# crypt32  — certificate handling behind WinVerifyTrust
target_link_libraries(boot PRIVATE bcrypt wintrust crypt32)

target_link_options(boot PRIVATE
    /SUBSYSTEM:WINDOWS
    "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/res/module.def"
    $<$<CONFIG:Release>:/OPT:REF>
    $<$<CONFIG:Release>:/OPT:ICF>
)

set_property(TARGET boot PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

set_source_files_properties(res/boot.rc PROPERTIES
    COMPILE_DEFINITIONS "PENGU_VERSION_COMMAS=${PENGU_VERSION_COMMAS};PENGU_VERSION_STR=\"${PENGU_VERSION}\""
)
