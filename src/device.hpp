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

#include "instance.hpp"
#ifdef _WIN32
#include <Volk/volk.h>
#else
#include <volk/volk.h>
#endif
#include <vulkan/vulkan_raii.hpp>
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
    //! Requires graphics + present queues and the VK_KHR_swapchain extension; discrete GPUs
    //! are preferred. vk::raii exceptions are caught in init() and translated to bool.
    class Device {
    public:
        Device() = default;
        ~Device();

        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(Device&&) = delete;

        //! Picks a physical device and creates the logical device + queues. Returns false and
        //! fills out_error_message on failure.
        [[nodiscard]] bool init(const Instance& instance, const vk::raii::SurfaceKHR& surface, std::string& out_error_message);

        //! Destroys the logical device. Safe to call repeatedly.
        void destroy();

        [[nodiscard]] const vk::raii::PhysicalDevice& physicalDevice() const
        {
            return m_physical_device;
        }

        [[nodiscard]] const vk::raii::Device& get() const
        {
            return m_device;
        }

        [[nodiscard]] const vk::raii::Queue& graphicsQueue() const
        {
            return m_graphics_queue;
        }

        [[nodiscard]] const vk::raii::Queue& presentQueue() const
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
        [[nodiscard]] static QueueFamilyIndices findQueueFamilies(const vk::raii::PhysicalDevice& physical_device, const vk::raii::SurfaceKHR& surface);

        //! Scores a physical device for suitability; returns a negative value when unusable.
        [[nodiscard]] static int rateDevice(const vk::raii::PhysicalDevice& physical_device, const vk::raii::SurfaceKHR& surface);

        vk::raii::PhysicalDevice m_physical_device{nullptr}; //!< Chosen physical device.
        vk::raii::Device m_device{nullptr}; //!< Logical device handle.
        vk::raii::Queue m_graphics_queue{nullptr}; //!< Graphics queue handle.
        vk::raii::Queue m_present_queue{nullptr}; //!< Present queue handle.
        QueueFamilyIndices m_queue_families{}; //!< Selected queue family indices.
        std::string m_device_name; //!< Human-readable name of the chosen device.
    };

} // namespace Engine
