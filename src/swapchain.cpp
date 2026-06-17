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

#include "swapchain.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <ranges>

namespace Engine
{

    //! Selects the surface format, preferring 8-bit BGRA sRGB (the conventional choice for a
    //! colour attachment the display interprets as sRGB).
    [[nodiscard]] static vk::SurfaceFormatKHR chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available)
    {
        std::vector<vk::SurfaceFormatKHR>::const_iterator it{std::ranges::find_if(available, [](const vk::SurfaceFormatKHR& fmt) {
            return (fmt.format == vk::Format::eB8G8R8A8Srgb) && (fmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
        })};
        if (it != available.end()) {
            return *it;
        }
        return available.front();
    }

    //! Selects the present mode. FIFO (v-sync): steady frame pacing for stable physics and easy
    //! on integrated GPUs / laptop battery. Always available per the Vulkan spec.
    [[nodiscard]] static vk::PresentModeKHR choosePresentMode([[maybe_unused]] const std::vector<vk::PresentModeKHR>& available)
    {
        return vk::PresentModeKHR::eFifo;
    }

    //! Clamps the requested dimensions to the surface's allowed range.
    [[nodiscard]] static vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        vk::Extent2D extent{};
        extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return extent;
    }

    bool Swapchain::init(const Device& device, vk::SurfaceKHR surface, uint32_t width, uint32_t height, std::string& out_error_message)
    {
        m_device = &device;
        m_surface = surface;
        try {
            build(width, height);
        } catch (const vk::SystemError& e) {
            out_error_message = std::string("Vulkan error creating swapchain: ") + e.what();
            return false;
        } catch (const std::exception& e) {
            out_error_message = std::string("Error creating swapchain: ") + e.what();
            return false;
        }
        return true;
    }

    void Swapchain::recreate(uint32_t width, uint32_t height)
    {
        // Wait for all GPU work (including present) to finish before destroying the old
        // swapchain — fences only track command-buffer completion, not present.
        m_device->get().waitIdle();
        m_views.clear();
        m_images.clear();
        build(width, height);
    }

    void Swapchain::destroy()
    {
        m_views.clear();
        m_images.clear();
        m_swapchain = nullptr;
    }

    void Swapchain::build(uint32_t width, uint32_t height)
    {
        // Zero extent (minimised window): Vulkan requires imageExtent > 0. Leave the extent
        // zero so callers skip rendering, and keep no images/views.
        if ((width == 0) || (height == 0)) {
            m_extent = vk::Extent2D{0, 0};
            return;
        }

        const vk::raii::PhysicalDevice& physical_device{m_device->physicalDevice()};

        vk::SurfaceCapabilitiesKHR capabilities{physical_device.getSurfaceCapabilitiesKHR(m_surface)};
        std::vector<vk::SurfaceFormatKHR> formats{physical_device.getSurfaceFormatsKHR(m_surface)};
        std::vector<vk::PresentModeKHR> present_modes{physical_device.getSurfacePresentModesKHR(m_surface)};

        m_format = chooseSurfaceFormat(formats);
        m_present_mode = choosePresentMode(present_modes);
        m_extent = chooseExtent(capabilities, width, height);

        // One extra image over the minimum for triple-buffering headroom.
        uint32_t image_count{capabilities.minImageCount + 1};
        if ((capabilities.maxImageCount > 0) && (image_count > capabilities.maxImageCount)) {
            image_count = capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR create_info{};
        create_info.surface = m_surface;
        create_info.minImageCount = image_count;
        create_info.imageFormat = m_format.format;
        create_info.imageColorSpace = m_format.colorSpace;
        create_info.imageExtent = m_extent;
        create_info.imageArrayLayers = 1;
        create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

        uint32_t graphics_family{m_device->queueFamilies().graphics};
        uint32_t present_family{m_device->queueFamilies().present};
        std::array<uint32_t, 2> family_indices{graphics_family, present_family};

        if (graphics_family != present_family) {
            create_info.imageSharingMode = vk::SharingMode::eConcurrent;
            create_info.setQueueFamilyIndices(family_indices);
        } else {
            create_info.imageSharingMode = vk::SharingMode::eExclusive;
        }

        create_info.preTransform = capabilities.currentTransform;
        create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        create_info.presentMode = m_present_mode;
        create_info.clipped = vk::True;
        create_info.oldSwapchain = *m_swapchain; // VK_NULL_HANDLE on first build; old one on recreate.

        m_swapchain = vk::raii::SwapchainKHR(m_device->get(), create_info);
        m_images = m_swapchain.getImages();

        m_views.clear();
        m_views.reserve(m_images.size());
        for (vk::Image image : m_images) {
            vk::ImageViewCreateInfo view_info{};
            view_info.image = image;
            view_info.viewType = vk::ImageViewType::e2D;
            view_info.format = m_format.format;
            view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
            view_info.subresourceRange.baseMipLevel = 0;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.baseArrayLayer = 0;
            view_info.subresourceRange.layerCount = 1;
            m_views.push_back(vk::raii::ImageView(m_device->get(), view_info));
        }
    }

} // namespace Engine
