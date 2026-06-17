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

#include <volk/volk.h>
#include <cstdint>
#include <string>

namespace Engine
{

    //! Graphics and present queue family indices for a physical device.
    struct QueueFamilyIndices {
        uint32_t graphics{0}; //!< Graphics-capable queue family index.
        uint32_t present{0}; //!< Presentation-capable queue family index.
        bool has_graphics{false}; //!< True once a graphics family has been found.
        bool has_present{false}; //!< True once a present family has been found.

        //! Returns true when both a graphics and a present family are available.
        [[nodiscard]] bool isComplete() const
        {
            return has_graphics && has_present;
        }
    };

    //! Selects a suitable physical device and owns the logical device + queue handles.
    //! Requires a graphics queue, a present queue (for the given surface) and the
    //! VK_KHR_swapchain extension. Discrete GPUs are preferred over integrated ones.
    class Device {
    public:
        Device() = default;
        ~Device();

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(Device&&) = delete;

        //! Picks a physical device and creates the logical device + queues. Returns false
        //! and fills out_error_message on failure.
        [[nodiscard]] bool init(VkInstance instance, VkSurfaceKHR surface, std::string& out_error_message);

        //! Destroys the logical device. Safe to call repeatedly.
        void destroy();

        [[nodiscard]] VkPhysicalDevice physicalDevice() const
        {
            return m_physical_device;
        }

        [[nodiscard]] VkDevice get() const
        {
            return m_vk_logical_device;
        }

        [[nodiscard]] VkQueue graphicsQueue() const
        {
            return m_graphics_queue;
        }

        [[nodiscard]] VkQueue presentQueue() const
        {
            return m_present_queue;
        }

        [[nodiscard]] const QueueFamilyIndices& queueFamilies() const
        {
            return m_queue_families;
        }

        [[nodiscard]] const std::string& name() const
        {
            return m_device_name;
        }

    private:
        //! Finds graphics + present queue families for a physical device against a surface.
        [[nodiscard]] static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

        //! Scores a physical device for suitability. Returns a negative value when the
        //! device cannot be used (missing queues or required extensions).
        [[nodiscard]] static int rateDevice(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

        VkPhysicalDevice m_physical_device{VK_NULL_HANDLE}; //!< Chosen physical device.
        VkDevice m_vk_logical_device{VK_NULL_HANDLE}; //!< Logical device handle.
        VkQueue m_graphics_queue{VK_NULL_HANDLE}; //!< Graphics queue handle.
        VkQueue m_present_queue{VK_NULL_HANDLE}; //!< Present queue handle.
        QueueFamilyIndices m_queue_families{}; //!< Selected queue family indices.
        std::string m_device_name; //!< Human-readable name of the chosen device.
    };

} // namespace Engine
