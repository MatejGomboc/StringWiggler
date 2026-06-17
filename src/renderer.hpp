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

    //! Composition root for the Vulkan back end. Owns, in dependency order, the instance,
    //! surface, device, allocator, per-frame vertex buffers, swapchain, pipeline and the
    //! per-frame command/sync objects.
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

        //! Number of nodes (points) in the string.
        static constexpr uint32_t NODE_COUNT = 32;

        //! Initialises the Vulkan back end for the given native window at the given size.
        //! Returns false and fills out_error_message on failure. The logger must outlive the
        //! renderer.
        [[nodiscard]] bool init(LoggingLib::Logger& logger, const NativeWindowHandle& window_handle, uint32_t width, uint32_t height, std::string& out_error_message);

        //! Renders and presents one frame. The string's head is pinned to the cursor (given in
        //! window client pixels); the remaining nodes hang straight down (Phase 4 adds physics).
        //! width/height drive swapchain recreation (resize/minimise). Never throws.
        void drawFrame(uint32_t width, uint32_t height, int32_t cursor_x, int32_t cursor_y);

        //! Tears down the back end in reverse construction order (waits for the GPU first).
        void destroy();

    private:
        //! Creates the command pool, per-frame command buffers and synchronisation objects.
        [[nodiscard]] bool createFrameResources(std::string& out_error_message);

        //! Recreates the swapchain (and the per-image render-finished semaphores) at a new size.
        void recreateSwapchain(uint32_t width, uint32_t height);

        //! Computes the string node positions (NDC) for a head at the given cursor pixel.
        [[nodiscard]] std::array<MathLib::Vec2, NODE_COUNT> computeNodes(uint32_t width, uint32_t height, int32_t cursor_x, int32_t cursor_y) const;

        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2; //!< Frames the CPU may run ahead of the GPU.

        LoggingLib::Logger* m_logger{nullptr}; //!< Logger (non-owning), set in init().
        Instance m_instance; //!< Vulkan instance + debug messenger.
        vk::raii::SurfaceKHR m_surface{nullptr}; //!< Window surface.
        Device m_device; //!< Physical + logical device and queues.
        Allocator m_allocator; //!< VMA allocator.
        std::vector<AllocatedBuffer> m_vertex_buffers; //!< One mapped vertex buffer per frame-in-flight (destroyed before the allocator).
        Swapchain m_swapchain; //!< Swapchain + image views.
        Pipeline m_pipeline; //!< Graphics pipeline for the line.
        vk::raii::CommandPool m_command_pool{nullptr}; //!< Graphics command pool.
        std::vector<vk::raii::CommandBuffer> m_command_buffers; //!< One per frame-in-flight.
        std::vector<vk::raii::Semaphore> m_image_available; //!< Signalled when an image is acquired (per frame-in-flight).
        std::vector<vk::raii::Semaphore> m_render_finished; //!< Signalled when rendering is done (per swapchain image).
        std::vector<vk::raii::Fence> m_in_flight; //!< CPU/GPU frame fence (per frame-in-flight).
        uint32_t m_current_frame{0}; //!< Index into the frame-in-flight arrays.
        bool m_initialised{false}; //!< True once init() has succeeded.
    };

} // namespace Engine
