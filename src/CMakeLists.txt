add_executable(${PROJECT_NAME}
    main.cpp
    volk.cpp
    instance.cpp
    surface.cpp
    device.cpp
    renderer.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)

# Vulkan-specific definitions — scoped to this target only so the libraries stay
# Vulkan-agnostic (the window library deals only in void* native handles).
target_compile_definitions(${PROJECT_NAME} PRIVATE
    VK_NO_PROTOTYPES
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR NOMINMAX WIN32_LEAN_AND_MEAN>
    $<$<PLATFORM_ID:Linux>:VK_USE_PLATFORM_XCB_KHR>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    window
    logging
    math
    ${CMAKE_DL_LIBS}
)
