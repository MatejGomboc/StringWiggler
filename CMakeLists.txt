cmake_minimum_required(VERSION 3.16)

project(StringWiggler
    LANGUAGES CXX
    VERSION 0.1.0
    DESCRIPTION "A cross-platform wiggling string simulator"
    HOMEPAGE_URL "https://github.com/MatejGomboc/StringWiggler"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings — high level, warnings as errors, all compilers.
# /EHsc enables C++ exceptions, which the unit-test framework relies on (production
# code follows the no-exceptions, return-bool convention).
if(MSVC)
    add_compile_options(/EHsc /W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Define DEBUG in Debug configurations so the renderer's validation-layer code compiles in.
add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

# Only Windows and Linux are supported (macOS and Wayland were dropped to match the
# Win32 + XCB platform reach).
if(NOT (WIN32 OR (UNIX AND NOT APPLE)))
    message(FATAL_ERROR "Unsupported platform. Only Windows and Linux are supported.")
endif()

find_package(Vulkan REQUIRED)

enable_testing()

add_subdirectory(libs)
add_subdirectory(src)
