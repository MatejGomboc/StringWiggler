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

#include "device.hpp"
#ifdef _WIN32
#include <Volk/volk.h>
#else
#include <volk/volk.h>
#endif
#include <vulkan/vulkan_raii.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Engine
{

    //! Owns the Vulkan swapchain and its per-image views. Supports recreation on resize.
    //! Colour-attachment only (the toy renders directly; no compute/storage usage).
    class Swapchain {
    public:
        Swapchain() = default;
        ~Swapchain() = default;

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;
        Swapchain(Swapchain&&) = delete;
        Swapchain& operator=(Swapchain&&) = delete;

        //! Builds the swapchain for the given device + surface at the requested size. Returns
        //! false and fills out_error_message on failure (vk::raii exceptions caught here).
        [[nodiscard]] bool init(const Device& device, vk::SurfaceKHR surface, uint32_t width, uint32_t height, std::string& out_error_message);

        //! Rebuilds the swapchain (after a resize / out-of-date). Waits for the device to be
        //! idle first. May throw vk::SystemError — call from within the renderer's try/catch.
        void recreate(uint32_t width, uint32_t height);

        //! Releases the swapchain + views. Safe to call repeatedly.
        void destroy();

        [[nodiscard]] const vk::raii::SwapchainKHR& get() const
        {
            return m_swapchain;
        }

        [[nodiscard]] const std::vector<vk::Image>& images() const
        {
            return m_images;
        }

        [[nodiscard]] const std::vector<vk::raii::ImageView>& views() const
        {
            return m_views;
        }

        [[nodiscard]] vk::Format format() const
        {
            return m_format.format;
        }

        [[nodiscard]] vk::Extent2D extent() const
        {
            return m_extent;
        }

        [[nodiscard]] uint32_t imageCount() const
        {
            return static_cast<uint32_t>(m_images.size());
        }

        //! True when the extent is zero (e.g. minimised window) — callers should skip rendering.
        [[nodiscard]] bool isZeroExtent() const
        {
            return (m_extent.width == 0) || (m_extent.height == 0);
        }

    private:
        //! Queries the surface and builds the swapchain + image views. May throw.
        void build(uint32_t width, uint32_t height);

        const Device* m_device{nullptr}; //!< Back-pointer to the device (non-owning).
        vk::SurfaceKHR m_surface{nullptr}; //!< Surface handle (non-owning).
        vk::raii::SwapchainKHR m_swapchain{nullptr}; //!< Swapchain handle.
        std::vector<vk::Image> m_images; //!< Swapchain images (non-owning, owned by the swapchain).
        std::vector<vk::raii::ImageView> m_views; //!< Per-image colour views.
        vk::SurfaceFormatKHR m_format{}; //!< Chosen surface format.
        vk::PresentModeKHR m_present_mode{vk::PresentModeKHR::eFifo}; //!< Chosen present mode.
        vk::Extent2D m_extent{}; //!< Current extent.
        std::string m_last_log; //!< Most recent build description (for logging by the renderer).
    };

} // namespace Engine
