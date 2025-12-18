cmake_minimum_required(VERSION 3.16)

project(StringWiggler
    LANGUAGES CXX
    VERSION 0.1.0
    DESCRIPTION "TODO!!!"
    HOMEPAGE_URL "https://github.com/MatejGomboc/StringWiggler"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Vulkan SDK (for headers and validation layers)
find_package(Vulkan REQUIRED)

# Fetch volk - Vulkan meta-loader
include(FetchContent)
FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG        1.4.304
)
FetchContent_MakeAvailable(volk)

add_executable(${PROJECT_NAME}
    main.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Use volk instead of linking to vulkan-1
# VK_NO_PROTOTYPES is defined by linking to volk
target_link_libraries(${PROJECT_NAME} PRIVATE volk)
