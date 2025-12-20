cmake_minimum_required(VERSION 3.16)

project(StringWiggler
    LANGUAGES CXX
    VERSION 0.1.0
    DESCRIPTION "TODO!!!"
    HOMEPAGE_URL "https://github.com/MatejGomboc/StringWiggler"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

set(COMMON_SOURCES
    volk.cpp
    renderer.cpp
    logger.cpp
)

if(WIN32)
    set(PLATFORM_MAIN main_win32.cpp)
    set(PLATFORM_DEFINES VK_USE_PLATFORM_WIN32_KHR)
    set(PLATFORM_LIBS "")
    set(APP_TYPE WIN32)

elseif(APPLE)
    enable_language(OBJCXX)
    set(PLATFORM_MAIN main_macos.mm)
    set(PLATFORM_DEFINES VK_USE_PLATFORM_METAL_EXT)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(QUARTZCORE_LIBRARY QuartzCore REQUIRED)
    set(PLATFORM_LIBS ${COCOA_LIBRARY} ${QUARTZCORE_LIBRARY})
    set(APP_TYPE MACOSX_BUNDLE)

elseif(UNIX)
    # Linux display server option: X11 (default) or Wayland
    set(LINUX_DISPLAY_SERVER "X11" CACHE STRING "Linux display server (X11 or Wayland)")
    set_property(CACHE LINUX_DISPLAY_SERVER PROPERTY STRINGS "X11" "Wayland")

    if(LINUX_DISPLAY_SERVER STREQUAL "Wayland")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(WAYLAND REQUIRED wayland-client)
        pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
        pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)

        # Find wayland-scanner
        find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

        # Generate xdg-shell protocol code
        set(XDG_SHELL_PROTOCOL "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
        set(XDG_SHELL_CLIENT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
        set(XDG_SHELL_CODE "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c")

        add_custom_command(
            OUTPUT ${XDG_SHELL_CLIENT_HEADER}
            COMMAND ${WAYLAND_SCANNER} client-header ${XDG_SHELL_PROTOCOL} ${XDG_SHELL_CLIENT_HEADER}
            DEPENDS ${XDG_SHELL_PROTOCOL}
            COMMENT "Generating xdg-shell client header"
        )

        add_custom_command(
            OUTPUT ${XDG_SHELL_CODE}
            COMMAND ${WAYLAND_SCANNER} private-code ${XDG_SHELL_PROTOCOL} ${XDG_SHELL_CODE}
            DEPENDS ${XDG_SHELL_PROTOCOL}
            COMMENT "Generating xdg-shell protocol code"
        )

        set(PLATFORM_MAIN main_linux_wayland.cpp ${XDG_SHELL_CODE})
        set(PLATFORM_DEFINES VK_USE_PLATFORM_WAYLAND_KHR)
        set(PLATFORM_LIBS ${WAYLAND_LIBRARIES})
        set(PLATFORM_INCLUDES ${WAYLAND_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
    else()
        find_package(X11 REQUIRED)
        set(PLATFORM_MAIN main_linux_x11.cpp)
        set(PLATFORM_DEFINES VK_USE_PLATFORM_XLIB_KHR)
        set(PLATFORM_LIBS ${X11_LIBRARIES})
        set(PLATFORM_INCLUDES "")
    endif()

    set(APP_TYPE "")
endif()

add_executable(${PROJECT_NAME} ${APP_TYPE}
    ${COMMON_SOURCES}
    ${PLATFORM_MAIN}
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${Vulkan_INCLUDE_DIRS}"
    ${PLATFORM_INCLUDES}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    VK_NO_PROTOTYPES
    ${PLATFORM_DEFINES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${PLATFORM_LIBS}
    ${CMAKE_DL_LIBS}
)

# MinGW requires -municode for wWinMain entry point
if(MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -municode)
endif()

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.MatejGomboc.StringWiggler"
        MACOSX_BUNDLE_BUNDLE_NAME "StringWiggler"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()
