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

#include "device.hpp"
#include "vulkan_helpers.hpp"
#include <set>
#include <vector>

namespace Engine
{

    //! Device extensions every supported GPU must provide.
    static const std::vector<const char*> REQUIRED_DEVICE_EXTENSIONS{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    Device::~Device()
    {
        destroy();
    }

    QueueFamilyIndices Device::findQueueFamilies(const vk::raii::PhysicalDevice& physical_device, const vk::raii::SurfaceKHR& surface)
    {
        QueueFamilyIndices indices{};

        std::vector<vk::QueueFamilyProperties> families = physical_device.getQueueFamilyProperties();
        for (uint32_t i = 0; i < families.size(); ++i) {
            if (families[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                indices.graphics = i;
                indices.has_graphics = true;
            }

            if (physical_device.getSurfaceSupportKHR(i, *surface)) {
                indices.present = i;
                indices.has_present = true;
            }

            if (indices.isComplete()) {
                break;
            }
        }

        return indices;
    }

    int Device::rateDevice(const vk::raii::PhysicalDevice& physical_device, const vk::raii::SurfaceKHR& surface)
    {
        QueueFamilyIndices indices = findQueueFamilies(physical_device, surface);
        if (!indices.isComplete()) {
            return -1;
        }

        std::vector<vk::ExtensionProperties> available = physical_device.enumerateDeviceExtensionProperties();
        for (const char* extension : REQUIRED_DEVICE_EXTENSIONS) {
            if (!isExtensionAvailable(available, extension)) {
                return -1;
            }
        }

        vk::PhysicalDeviceProperties properties = physical_device.getProperties();
        // Require Vulkan 1.3 — dynamicRendering and synchronization2 are core (mandatory)
        // features there, so any 1.3 device is guaranteed to support what we enable below.
        if (properties.apiVersion < VK_API_VERSION_1_3) {
            return -1;
        }
        int score = 0;
        if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            score += 1000;
        } else if (properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
            score += 100;
        }
        // Prefer a single family handling both graphics and present (simpler sharing mode).
        if (indices.graphics == indices.present) {
            score += 10;
        }
        return score;
    }

    bool Device::init(const Instance& instance, const vk::raii::SurfaceKHR& surface, std::string& out_error_message)
    {
        try {
            std::vector<vk::raii::PhysicalDevice> physical_devices = instance.get().enumeratePhysicalDevices();
            if (physical_devices.empty()) {
                out_error_message = "No Vulkan physical devices found.";
                return false;
            }

            int best_score = -1;
            int best_index = -1;
            for (size_t i = 0; i < physical_devices.size(); ++i) {
                int score = rateDevice(physical_devices[i], surface);
                if (score > best_score) {
                    best_score = score;
                    best_index = static_cast<int>(i);
                }
            }

            if ((best_index < 0) || (best_score < 0)) {
                out_error_message = "No supported Vulkan physical device found (need graphics + present queues and swapchain support).";
                return false;
            }

            m_physical_device = std::move(physical_devices[static_cast<size_t>(best_index)]);

            vk::PhysicalDeviceProperties properties = m_physical_device.getProperties();
            m_device_name = properties.deviceName.data();

            m_queue_families = findQueueFamilies(m_physical_device, surface);

            // Deduplicate queue family indices so each family is requested once.
            std::set<uint32_t> unique_families{m_queue_families.graphics, m_queue_families.present};
            float queue_priority = 1.0f;
            std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
            for (uint32_t family : unique_families) {
                vk::DeviceQueueCreateInfo queue_create_info{};
                queue_create_info.queueFamilyIndex = family;
                queue_create_info.queueCount = 1;
                queue_create_info.pQueuePriorities = &queue_priority;
                queue_create_infos.push_back(queue_create_info);
            }

            vk::PhysicalDeviceFeatures enabled_features{};

            // Vulkan 1.3 core features the renderer relies on: dynamic rendering (no render
            // pass / framebuffers) and synchronization2 (the pipelineBarrier2 / submit2 API).
            vk::PhysicalDeviceVulkan13Features features13{};
            features13.dynamicRendering = vk::True;
            features13.synchronization2 = vk::True;

            vk::DeviceCreateInfo device_create_info{};
            device_create_info.setQueueCreateInfos(queue_create_infos);
            device_create_info.setPEnabledExtensionNames(REQUIRED_DEVICE_EXTENSIONS);
            device_create_info.setPEnabledFeatures(&enabled_features);
            device_create_info.setPNext(&features13);

            m_device = vk::raii::Device(m_physical_device, device_create_info);

            volkLoadDevice(*m_device);
            VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_device);

            m_graphics_queue = m_device.getQueue(m_queue_families.graphics, 0);
            m_present_queue = m_device.getQueue(m_queue_families.present, 0);
        } catch (const vk::SystemError& e) {
            out_error_message = std::string("Vulkan error during device creation: ") + e.what();
            return false;
        } catch (const std::exception& e) {
            out_error_message = std::string("Error during device creation: ") + e.what();
            return false;
        }

        return true;
    }

    void Device::destroy()
    {
        // Assigning nullptr to a vk::raii handle destroys it (queues are non-owning, but
        // resetting keeps state consistent). Logical device last.
        m_present_queue = nullptr;
        m_graphics_queue = nullptr;
        m_device = nullptr;
        m_physical_device = nullptr;
    }

} // namespace Engine
