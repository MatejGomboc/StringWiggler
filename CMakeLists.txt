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
    find_package(X11 REQUIRED)
    set(PLATFORM_MAIN main_linux.cpp)
    set(PLATFORM_DEFINES VK_USE_PLATFORM_XLIB_KHR)
    set(PLATFORM_LIBS ${X11_LIBRARIES})
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
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    VK_NO_PROTOTYPES
    ${PLATFORM_DEFINES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${PLATFORM_LIBS}
    ${CMAKE_DL_LIBS}
)

if(APPLE)
    # macOS-specific bundle settings
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.github.MatejGomboc.StringWiggler"
        MACOSX_BUNDLE_BUNDLE_NAME "StringWiggler"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()
