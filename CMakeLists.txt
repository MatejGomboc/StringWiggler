cmake_minimum_required(VERSION 3.16)

project(StringWiggler
    LANGUAGES C CXX
    VERSION 0.1.0
    DESCRIPTION "TODO!!!"
    HOMEPAGE_URL "https://github.com/MatejGomboc/StringWiggler"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Vulkan SDK via VULKAN_SDK environment variable
find_package(Vulkan REQUIRED)

# Volk source files from the Vulkan SDK
set(VOLK_SOURCES "${Vulkan_INCLUDE_DIR}/volk/volk.c")
set(VOLK_HEADERS "${Vulkan_INCLUDE_DIR}/volk/volk.h")

if(NOT EXISTS "${VOLK_SOURCES}")
    message(FATAL_ERROR "volk.c not found in Vulkan SDK at: ${VOLK_SOURCES}")
endif()

# Create volk static library
add_library(volk STATIC ${VOLK_SOURCES} ${VOLK_HEADERS})
target_include_directories(volk PUBLIC "${Vulkan_INCLUDE_DIR}")
target_compile_definitions(volk PUBLIC VK_NO_PROTOTYPES)

# Platform-specific linking for volk
if(WIN32)
    # Nothing extra needed on Windows, volk loads vulkan-1.dll dynamically
elseif(APPLE)
    # Link to dl for dlopen/dlsym
    target_link_libraries(volk PRIVATE ${CMAKE_DL_LIBS})
else()
    # Linux: link to dl for dlopen/dlsym
    target_link_libraries(volk PRIVATE ${CMAKE_DL_LIBS})
endif()

add_executable(${PROJECT_NAME}
    main.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Link to volk (provides Vulkan headers + dynamic loading)
target_link_libraries(${PROJECT_NAME} PRIVATE volk)
