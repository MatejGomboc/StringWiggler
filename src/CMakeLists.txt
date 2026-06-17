add_executable(${PROJECT_NAME}
    main.cpp
    volk.cpp
    vma.cpp
    instance.cpp
    surface.cpp
    device.cpp
    allocator.cpp
    swapchain.cpp
    pipeline.cpp
    compute_pipeline.cpp
    shader_loader.cpp
    renderer.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)

# Vulkan-specific definitions — scoped to this target only so the libraries stay
# Vulkan-agnostic (the window library deals only in void* native handles).
# - VK_NO_PROTOTYPES: Volk provides the entry points dynamically.
# - VULKAN_HPP_DISPATCH_LOADER_DYNAMIC: vulkan-hpp / vk::raii use the dynamic dispatcher.
# - VMA_STATIC/DYNAMIC_VULKAN_FUNCTIONS=0: VMA takes its function pointers from Volk.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    VK_NO_PROTOTYPES
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=0
    $<$<PLATFORM_ID:Windows>:VK_USE_PLATFORM_WIN32_KHR NOMINMAX WIN32_LEAN_AND_MEAN>
    $<$<PLATFORM_ID:Linux>:VK_USE_PLATFORM_XCB_KHR>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    window
    logging
    signals
    math
    ${CMAKE_DL_LIBS}
)

# --- Shaders: Slang -> SPIR-V via slangc, validated with spirv-val (both from the Vulkan SDK) ---
find_program(SLANGC_EXECUTABLE slangc HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin" REQUIRED)
find_program(SPIRV_VAL_EXECUTABLE spirv-val HINTS "$ENV{VULKAN_SDK}/bin" "$ENV{VULKAN_SDK}/Bin" REQUIRED)

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(LINE_SHADER ${CMAKE_CURRENT_SOURCE_DIR}/line.slang)
set(PHYSICS_SHADER ${CMAKE_CURRENT_SOURCE_DIR}/physics.slang)

# Compile line.slang (both entry points) into one SPIR-V module, then validate it.
add_custom_command(
    OUTPUT ${SHADER_OUTPUT_DIR}/line.spv
    COMMAND ${SLANGC_EXECUTABLE}
        ${LINE_SHADER}
        -target spirv
        -profile spirv_1_4
        -emit-spirv-directly
        -fvk-use-entrypoint-name
        -warnings-as-errors all
        -entry vertMain
        -entry fragMain
        -o ${SHADER_OUTPUT_DIR}/line.spv
    COMMAND ${SPIRV_VAL_EXECUTABLE} --target-env vulkan1.3 ${SHADER_OUTPUT_DIR}/line.spv
    DEPENDS ${LINE_SHADER}
    COMMENT "Compiling line.slang -> line.spv (with validation)"
    VERBATIM
)

# Compile physics.slang (compute) into a SPIR-V module, then validate it.
add_custom_command(
    OUTPUT ${SHADER_OUTPUT_DIR}/physics.spv
    COMMAND ${SLANGC_EXECUTABLE}
        ${PHYSICS_SHADER}
        -target spirv
        -profile spirv_1_4
        -emit-spirv-directly
        -fvk-use-entrypoint-name
        -warnings-as-errors all
        -entry physicsMain
        -o ${SHADER_OUTPUT_DIR}/physics.spv
    COMMAND ${SPIRV_VAL_EXECUTABLE} --target-env vulkan1.3 ${SHADER_OUTPUT_DIR}/physics.spv
    DEPENDS ${PHYSICS_SHADER}
    COMMENT "Compiling physics.slang -> physics.spv (with validation)"
    VERBATIM
)

# Compile shaders as part of ALL, and copy them next to the executable for runtime loading.
add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUT_DIR}/line.spv ${SHADER_OUTPUT_DIR}/physics.spv)
add_dependencies(${PROJECT_NAME} shaders)

add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_OUTPUT_DIR}/line.spv $<TARGET_FILE_DIR:${PROJECT_NAME}>/line.spv
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_OUTPUT_DIR}/physics.spv $<TARGET_FILE_DIR:${PROJECT_NAME}>/physics.spv
    COMMENT "Copying shaders next to the executable"
    VERBATIM
)
add_dependencies(copy_shaders shaders)
