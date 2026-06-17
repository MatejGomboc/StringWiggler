/*
    Copyright (C) 2025 Matej Gomboc https://github.com/MatejGomboc/StringWiggler

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
*/

#pragma once

#include "allocator.hpp"
#include "compute_pipeline.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "native_window_handle.hpp"
#include "pipeline.hpp"
#include "swapchain.hpp"
#include <log/logger.hpp>
#include <math/vector.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Engine
{

    //! Composition root for the Vulkan back end. The string is simulated on the GPU by a
    //! compute shader (Verlet + distance constraints) writing the positions buffer, which the
    //! graphics pipeline then draws as a line strip.
    //!
    //! Exception policy: vk::raii throws on Vulkan errors; those are caught at the init() and
    //! drawFrame() boundaries (and in each sub-component) and never escape the public API.
    class Renderer {
    public:
        Renderer() = default;
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        //! Number of nodes (points) in the string. Must match NODE_COUNT in physics.slang.
        static constexpr uint32_t NODE_COUNT = 128;

        //! Initialises the Vulkan back end for the given native window at the given size.
        //! Returns false and fills out_error_message on failure. The logger must outlive the
        //! renderer.
        [[nodiscard]] bool init(LoggingLib::Logger& logger, const NativeWindowHandle& window_handle, uint32_t width, uint32_t height, std::string& out_error_message);

        //! Simulates one physics step on the GPU (head pinned to the cursor, given in window
        //! client pixels) and renders the string. dt is the frame delta time in seconds.
        //! width/height drive swapchain recreation (resize/minimise). Never throws.
        void drawFrame(uint32_t width, uint32_t height, int32_t cursor_x, int32_t cursor_y, float dt);

        //! Tears down the back end in reverse construction order (waits for the GPU first).
        void destroy();

    private:
        //! Creates the command pool, per-frame command buffers and synchronisation objects.
        [[nodiscard]] bool createFrameResources(std::string& out_error_message);

        //! Creates + fills the positions/previous-positions storage buffers and the compute
        //! descriptor set.
        [[nodiscard]] bool createPhysicsResources(std::string& out_error_message);

        //! Recreates the swapchain (and the per-image render-finished semaphores) at a new size.
        void recreateSwapchain(uint32_t width, uint32_t height);

        // One frame in flight: the physics state lives in a single GPU buffer shared by compute
        // and the vertex stage, so a single in-flight frame avoids a cross-frame data race and
        // is plenty at v-sync.
        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 1; //!< Frames the CPU may run ahead of the GPU.

        LoggingLib::Logger* m_logger{nullptr}; //!< Logger (non-owning), set in init().
        Instance m_instance; //!< Vulkan instance + debug messenger.
        vk::raii::SurfaceKHR m_surface{nullptr}; //!< Window surface.
        Device m_device; //!< Physical + logical device and queues.
        Allocator m_allocator; //!< VMA allocator.
        AllocatedBuffer m_positions; //!< Current node positions (storage + vertex buffer; before allocator).
        AllocatedBuffer m_prev_positions; //!< Previous node positions (Verlet history; before allocator).
        Swapchain m_swapchain; //!< Swapchain + image views.
        Pipeline m_pipeline; //!< Graphics pipeline (draws the line strip).
        ComputePipeline m_compute_pipeline; //!< Compute pipeline (physics).
        vk::raii::DescriptorPool m_descriptor_pool{nullptr}; //!< Pool for the compute descriptor set.
        vk::raii::DescriptorSet m_descriptor_set{nullptr}; //!< Binds positions + prev to the compute shader.
        vk::raii::CommandPool m_command_pool{nullptr}; //!< Graphics/compute command pool.
        std::vector<vk::raii::CommandBuffer> m_command_buffers; //!< One per frame-in-flight.
        std::vector<vk::raii::Semaphore> m_image_available; //!< Signalled when an image is acquired (per frame-in-flight).
        std::vector<vk::raii::Semaphore> m_render_finished; //!< Signalled when rendering is done (per swapchain image).
        std::vector<vk::raii::Fence> m_in_flight; //!< CPU/GPU frame fence (per frame-in-flight).
        uint32_t m_current_frame{0}; //!< Index into the frame-in-flight arrays.
        bool m_initialised{false}; //!< True once init() has succeeded.
    };

} // namespace Engine
