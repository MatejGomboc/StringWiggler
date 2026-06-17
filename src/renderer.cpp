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

#include "renderer.hpp"
#include "surface.hpp"
#include <array>
#include <cstdint>
#include <cstring>

namespace Engine
{

    //! Background clear colour. A dark blue (not black) so the rendered frame is visibly
    //! distinct from the window's black fill. The final string-toy background will be black.
    static constexpr std::array<float, 4> CLEAR_COLOUR{0.05f, 0.05f, 0.15f, 1.0f};

    //! Total length of the (rigid, for now) string in normalised device coordinates.
    static constexpr float STRING_LENGTH_NDC = 1.6f;

    Renderer::~Renderer()
    {
        destroy();
    }

    std::array<MathLib::Vec2, Renderer::NODE_COUNT> Renderer::computeNodes(uint32_t width, uint32_t height, int32_t cursor_x, int32_t cursor_y) const
    {
        // Map the cursor (window client pixels) to NDC. Vulkan clip space is +Y down, which
        // matches screen-space pixels, so no flip is needed: cursor at the top -> y near -1.
        float ndc_x = (width > 0) ? ((static_cast<float>(cursor_x) / static_cast<float>(width)) * 2.0f - 1.0f) : 0.0f;
        float ndc_y = (height > 0) ? ((static_cast<float>(cursor_y) / static_cast<float>(height)) * 2.0f - 1.0f) : 0.0f;
        MathLib::Vec2 head{ndc_x, ndc_y};

        // Rigid hang: each node sits a fixed step below the previous one. Phase 4 will replace
        // this with Verlet integration so the lower nodes lag and swing.
        constexpr float segment = STRING_LENGTH_NDC / static_cast<float>(NODE_COUNT - 1);
        std::array<MathLib::Vec2, NODE_COUNT> nodes{};
        for (uint32_t i = 0; i < NODE_COUNT; ++i) {
            nodes[i] = head + MathLib::Vec2{0.0f, static_cast<float>(i) * segment};
        }
        return nodes;
    }

    bool Renderer::init(LoggingLib::Logger& logger, const NativeWindowHandle& window_handle, uint32_t width, uint32_t height, std::string& out_error_message)
    {
        if (m_initialised) {
            out_error_message = "Renderer already initialised.";
            return false;
        }
        m_logger = &logger;

        try {
            if (!m_instance.init(logger, out_error_message)) {
                destroy();
                return false;
            }
            logger.logInfo("Vulkan instance created.");

            if (!createSurface(m_instance.get(), window_handle, m_surface, out_error_message)) {
                destroy();
                return false;
            }

            if (!m_device.init(m_instance, m_surface, out_error_message)) {
                destroy();
                return false;
            }
            logger.logInfo("Selected GPU \"" + m_device.name() + "\" for rendering.");

            if (!m_allocator.init(m_instance.handle(), *m_device.physicalDevice(), *m_device.get(), logger, out_error_message)) {
                destroy();
                return false;
            }

            if (!m_swapchain.init(m_device, *m_surface, width, height, out_error_message)) {
                destroy();
                return false;
            }
            logger.logInfo("Swapchain created: " + std::to_string(m_swapchain.extent().width) + "x" + std::to_string(m_swapchain.extent().height) + ".");

            if (!m_pipeline.init(m_device, m_swapchain.format(), out_error_message)) {
                destroy();
                return false;
            }

            // One host-visible, persistently-mapped vertex buffer per frame-in-flight, so the
            // string can be rewritten each frame without racing the GPU's read of another frame.
            VkDeviceSize vertex_size = NODE_COUNT * sizeof(MathLib::Vec2);
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                m_vertex_buffers.push_back(m_allocator.createBuffer(vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, VMA_MEMORY_USAGE_AUTO));
            }

            if (!createFrameResources(out_error_message)) {
                destroy();
                return false;
            }
        } catch (const vk::SystemError& e) {
            out_error_message = std::string("Vulkan error during renderer initialisation: ") + e.what();
            destroy();
            return false;
        } catch (const std::exception& e) {
            out_error_message = std::string("Error during renderer initialisation: ") + e.what();
            destroy();
            return false;
        }

        m_initialised = true;
        return true;
    }

    bool Renderer::createFrameResources(std::string& out_error_message)
    {
        try {
            const vk::raii::Device& device = m_device.get();

            vk::CommandPoolCreateInfo pool_info{};
            pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
            pool_info.queueFamilyIndex = m_device.queueFamilies().graphics;
            m_command_pool = vk::raii::CommandPool(device, pool_info);

            vk::CommandBufferAllocateInfo alloc_info{};
            alloc_info.commandPool = *m_command_pool;
            alloc_info.level = vk::CommandBufferLevel::ePrimary;
            alloc_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
            m_command_buffers = device.allocateCommandBuffers(alloc_info);

            m_image_available.clear();
            m_in_flight.clear();
            for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                m_image_available.push_back(vk::raii::Semaphore(device, vk::SemaphoreCreateInfo{}));
                vk::FenceCreateInfo fence_info{};
                fence_info.flags = vk::FenceCreateFlagBits::eSignaled; // start signalled so the first wait returns immediately.
                m_in_flight.push_back(vk::raii::Fence(device, fence_info));
            }

            // render-finished semaphores are per swapchain image (avoids reusing a semaphore
            // still tied to an in-flight present).
            m_render_finished.clear();
            for (uint32_t i = 0; i < m_swapchain.imageCount(); ++i) {
                m_render_finished.push_back(vk::raii::Semaphore(device, vk::SemaphoreCreateInfo{}));
            }
        } catch (const vk::SystemError& e) {
            out_error_message = std::string("Vulkan error creating frame resources: ") + e.what();
            return false;
        }
        return true;
    }

    void Renderer::recreateSwapchain(uint32_t width, uint32_t height)
    {
        m_swapchain.recreate(width, height);
        // The per-image render-finished semaphores must match the (possibly new) image count.
        m_render_finished.clear();
        for (uint32_t i = 0; i < m_swapchain.imageCount(); ++i) {
            m_render_finished.push_back(vk::raii::Semaphore(m_device.get(), vk::SemaphoreCreateInfo{}));
        }
    }

    void Renderer::drawFrame(uint32_t width, uint32_t height, int32_t cursor_x, int32_t cursor_y)
    {
        if (!m_initialised) {
            return;
        }

        // Minimised / zero-extent: nothing to render. Try to rebuild if the window came back.
        if (m_swapchain.isZeroExtent()) {
            if ((width > 0) && (height > 0)) {
                try {
                    recreateSwapchain(width, height);
                } catch (const vk::SystemError&) {
                    // Still not presentable; try again next frame.
                }
            }
            return;
        }

        try {
            const vk::raii::Device& device = m_device.get();

            // 1. Wait for this frame slot's previous work to finish (also makes its vertex
            //    buffer safe to overwrite).
            (void)device.waitForFences({*m_in_flight[m_current_frame]}, vk::True, UINT64_MAX);

            // 2. Acquire the next image (throws vk::OutOfDateKHRError if the swapchain is stale).
            vk::ResultValue<uint32_t> acquire = m_swapchain.get().acquireNextImage(UINT64_MAX, *m_image_available[m_current_frame]);
            uint32_t image_index = acquire.value;
            bool suboptimal = (acquire.result == vk::Result::eSuboptimalKHR);

            // 3. Reset the fence only now (after a successful acquire) to avoid a deadlock.
            device.resetFences({*m_in_flight[m_current_frame]});

            // 4. Update this frame's vertex buffer with the current string nodes.
            std::array<MathLib::Vec2, NODE_COUNT> nodes = computeNodes(width, height, cursor_x, cursor_y);
            std::memcpy(m_vertex_buffers[m_current_frame].allocationInfo().pMappedData, nodes.data(), sizeof(nodes));

            // 5. Record the frame.
            const vk::raii::CommandBuffer& cmd = m_command_buffers[m_current_frame];
            cmd.reset();
            vk::CommandBufferBeginInfo begin_info{};
            cmd.begin(begin_info);

            vk::ImageSubresourceRange colour_range{};
            colour_range.aspectMask = vk::ImageAspectFlagBits::eColor;
            colour_range.baseMipLevel = 0;
            colour_range.levelCount = 1;
            colour_range.baseArrayLayer = 0;
            colour_range.layerCount = 1;

            // Barrier: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL.
            vk::ImageMemoryBarrier2 to_attachment{};
            to_attachment.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe;
            to_attachment.srcAccessMask = vk::AccessFlagBits2::eNone;
            to_attachment.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
            to_attachment.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
            to_attachment.oldLayout = vk::ImageLayout::eUndefined;
            to_attachment.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            to_attachment.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
            to_attachment.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
            to_attachment.image = m_swapchain.images()[image_index];
            to_attachment.subresourceRange = colour_range;

            vk::DependencyInfo dep_to_attachment{};
            dep_to_attachment.setImageMemoryBarriers(to_attachment);
            cmd.pipelineBarrier2(dep_to_attachment);

            vk::RenderingAttachmentInfo colour_attachment{};
            colour_attachment.imageView = *m_swapchain.views()[image_index];
            colour_attachment.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
            colour_attachment.loadOp = vk::AttachmentLoadOp::eClear;
            colour_attachment.storeOp = vk::AttachmentStoreOp::eStore;
            colour_attachment.clearValue.color = vk::ClearColorValue(CLEAR_COLOUR);

            vk::RenderingInfo rendering_info{};
            rendering_info.renderArea = vk::Rect2D{vk::Offset2D{0, 0}, m_swapchain.extent()};
            rendering_info.layerCount = 1;
            rendering_info.setColorAttachments(colour_attachment);

            cmd.beginRendering(rendering_info);

            vk::Viewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(m_swapchain.extent().width);
            viewport.height = static_cast<float>(m_swapchain.extent().height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            cmd.setViewport(0, viewport);

            vk::Rect2D scissor{vk::Offset2D{0, 0}, m_swapchain.extent()};
            cmd.setScissor(0, scissor);

            cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_pipeline.get());
            vk::Buffer vertex_buffer{m_vertex_buffers[m_current_frame].buffer()};
            vk::DeviceSize vertex_offset{0};
            cmd.bindVertexBuffers(0, vertex_buffer, vertex_offset);
            cmd.draw(NODE_COUNT, 1, 0, 0);

            cmd.endRendering();

            // Barrier: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC.
            vk::ImageMemoryBarrier2 to_present{};
            to_present.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;
            to_present.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite;
            to_present.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe;
            to_present.dstAccessMask = vk::AccessFlagBits2::eNone;
            to_present.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
            to_present.newLayout = vk::ImageLayout::ePresentSrcKHR;
            to_present.srcQueueFamilyIndex = vk::QueueFamilyIgnored;
            to_present.dstQueueFamilyIndex = vk::QueueFamilyIgnored;
            to_present.image = m_swapchain.images()[image_index];
            to_present.subresourceRange = colour_range;

            vk::DependencyInfo dep_to_present{};
            dep_to_present.setImageMemoryBarriers(to_present);
            cmd.pipelineBarrier2(dep_to_present);

            cmd.end();

            // 6. Submit (wait on image-available, signal render-finished, fence this frame slot).
            vk::CommandBufferSubmitInfo cmd_submit{};
            cmd_submit.commandBuffer = *cmd;

            vk::SemaphoreSubmitInfo wait_submit{};
            wait_submit.semaphore = *m_image_available[m_current_frame];
            wait_submit.stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;

            vk::SemaphoreSubmitInfo signal_submit{};
            signal_submit.semaphore = *m_render_finished[image_index];
            signal_submit.stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput;

            vk::SubmitInfo2 submit{};
            submit.setWaitSemaphoreInfos(wait_submit);
            submit.setCommandBufferInfos(cmd_submit);
            submit.setSignalSemaphoreInfos(signal_submit);

            m_device.graphicsQueue().submit2(submit, *m_in_flight[m_current_frame]);

            // 7. Present (waits on render-finished).
            vk::SwapchainKHR swapchain_handle = *m_swapchain.get();
            vk::Semaphore render_finished = *m_render_finished[image_index];
            vk::PresentInfoKHR present_info{};
            present_info.setWaitSemaphores(render_finished);
            present_info.setSwapchains(swapchain_handle);
            present_info.setImageIndices(image_index);

            vk::Result present_result = m_device.presentQueue().presentKHR(present_info);
            if (suboptimal || (present_result == vk::Result::eSuboptimalKHR)) {
                recreateSwapchain(width, height);
            }

            m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
        } catch (const vk::OutOfDateKHRError&) {
            // Swapchain went stale (resize/minimise) — rebuild and skip this frame.
            try {
                recreateSwapchain(width, height);
            } catch (const vk::SystemError& e) {
                if (m_logger) {
                    m_logger->logError(std::string("Failed to recreate swapchain: ") + e.what());
                }
            }
        } catch (const vk::SystemError& e) {
            if (m_logger) {
                m_logger->logError(std::string("Frame render failed: ") + e.what());
            }
        }
    }

    void Renderer::destroy()
    {
        // Ensure the GPU is idle before tearing anything down (frames may be in flight).
        try {
            if (*m_device.get()) {
                m_device.get().waitIdle();
            }
        } catch (const vk::SystemError&) {
            // Already failing; proceed with teardown regardless.
        }

        // Reverse construction order. Assigning nullptr to a vk::raii handle destroys it.
        m_in_flight.clear();
        m_render_finished.clear();
        m_image_available.clear();
        m_command_buffers.clear();
        m_command_pool = nullptr;
        m_pipeline.destroy();
        m_swapchain.destroy();
        m_vertex_buffers.clear(); // free the vertex buffers while the allocator is still alive.
        m_allocator.destroy();
        m_device.destroy();
        m_surface = nullptr;
        m_instance.destroy();
        m_initialised = false;
    }

} // namespace Engine
