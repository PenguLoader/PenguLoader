cmake_minimum_required(VERSION 3.20)

project(PenguLoader VERSION 1.2.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Platform-specific configurations
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
    # Use MSBuild for Windows
    message(STATUS "Windows detected - Consider using MSBuild for production builds")
elseif(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for macOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version")
endif()

# Add the core subdirectory
add_subdirectory(core)

# Install targets
install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/
        DESTINATION bin
        FILES_MATCHING PATTERN "*"
        PATTERN "obj" EXCLUDE)
