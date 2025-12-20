cmake_minimum_required(VERSION 3.16)

project(StringWiggler
    LANGUAGES CXX
    VERSION 0.1.0
    DESCRIPTION "TODO!!!"
    HOMEPAGE_URL "https://github.com/MatejGomboc/StringWiggler"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Vulkan SDK via VULKAN_SDK environment variable
find_package(Vulkan REQUIRED)

add_executable(${PROJECT_NAME}
    main.cpp
    volk.cpp
    renderer.cpp
    renderer.h
    logger.cpp
    logger.h
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${Vulkan_INCLUDE_DIRS}"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)

# Link dl library on Unix for volk's dlopen/dlsym usage
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
