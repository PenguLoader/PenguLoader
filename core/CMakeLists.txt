cmake_minimum_required(VERSION 3.20)

project(core VERSION 1.2.0 LANGUAGES C CXX)

# Set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/obj)

# Collect source files
file(GLOB_RECURSE CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cc"
)

# Collect macOS Objective-C++ sources separately
if(APPLE)
    file(GLOB_RECURSE CORE_OBJCXX_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.mm"
    )
endif()

# Include directories
set(CORE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/cef
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Platform-specific configurations
if(WIN32)
    # Windows: Build as DLL
    add_library(core SHARED ${CORE_SOURCES})
    
    target_compile_definitions(core PRIVATE
        UNICODE
        _UNICODE
    )
    
    target_compile_options(core PRIVATE
        /std:c++20
        /W3
    )
    
    # Link against Windows libraries
    target_link_libraries(core PRIVATE
        # Add Windows-specific libraries here if needed
    )
    
    # Set output name
    set_target_properties(core PROPERTIES
        OUTPUT_NAME "core"
        SUFFIX ".dll"
    )
    
elseif(APPLE)
    # macOS: Build as dylib
    add_library(core SHARED 
        ${CORE_SOURCES}
        ${CORE_OBJCXX_SOURCES}
    )
    
    target_compile_definitions(core PRIVATE
        DEBUG=$<CONFIG:Debug>
        NDEBUG=$<CONFIG:Release>
    )
    
    target_compile_options(core PRIVATE
        -std=c++20
        -fPIC
        -fvisibility=hidden
        -Wno-address-of-temporary
        -Wno-nonportable-include-path
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Debug>:-g>
    )
    
    # Objective-C++ specific flags
    set_source_files_properties(${CORE_OBJCXX_SOURCES}
        PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )
    
    # Link against macOS frameworks
    target_link_libraries(core PRIVATE
        "-framework Cocoa"
        "-Lcore/cef/lib/mac"
        "-weak-lcef.d"
        "-flat_namespace"
    )
    
    # Set macOS-specific properties
    set_target_properties(core PROPERTIES
        OUTPUT_NAME "core"
        SUFFIX ".dylib"
        MACOSX_RPATH TRUE
        OSX_ARCHITECTURES "x86_64;arm64"
    )
    
    # Additional linker flags for macOS
    set_target_properties(core PROPERTIES
        LINK_FLAGS "-dynamiclib -current_version ${PROJECT_VERSION} -compatibility_version 1.0.0"
    )
endif()

# Set include directories
target_include_directories(core PRIVATE ${CORE_INCLUDE_DIRS})

# Build insert_dylib tool for macOS
if(APPLE)
    add_executable(insert_dylib ${CMAKE_CURRENT_SOURCE_DIR}/insert_dylib.c)
    set_target_properties(insert_dylib PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
    )
    target_compile_options(insert_dylib PRIVATE -O2)
endif()
