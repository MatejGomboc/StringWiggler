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

    QueueFamilyIndices Device::findQueueFamilies(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
    {
        QueueFamilyIndices indices{};

        uint32_t queue_families_count{0};
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_families_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, queue_families.data());

        for (uint32_t i{0}; i < queue_families_count; ++i) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphics = i;
                indices.has_graphics = true;
            }

            VkBool32 present_supported{VK_FALSE};
            if (vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_supported) == VK_SUCCESS && present_supported) {
                indices.present = i;
                indices.has_present = true;
            }

            if (indices.isComplete()) {
                break;
            }
        }

        return indices;
    }

    int Device::rateDevice(VkPhysicalDevice physical_device, VkSurfaceKHR surface)
    {
        QueueFamilyIndices indices{findQueueFamilies(physical_device, surface)};
        if (!indices.isComplete()) {
            return -1;
        }

        // Require all device extensions (swapchain).
        uint32_t extension_count{0};
        if (vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr) != VK_SUCCESS) {
            return -1;
        }
        std::vector<VkExtensionProperties> available(extension_count);
        if (vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, available.data()) != VK_SUCCESS) {
            return -1;
        }
        for (const char* const extension : REQUIRED_DEVICE_EXTENSIONS) {
            if (!isExtensionAvailable(available, extension)) {
                return -1;
            }
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical_device, &properties);

        int score{0};
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 100;
        }
        // Prefer a single family that handles both graphics and present (simpler sharing mode).
        if (indices.graphics == indices.present) {
            score += 10;
        }
        return score;
    }

    bool Device::init(VkInstance instance, VkSurfaceKHR surface, std::string& out_error_message)
    {
        if (m_vk_logical_device != VK_NULL_HANDLE) {
            out_error_message = "Vulkan logical device already created.";
            return false;
        }

        uint32_t physical_devices_count{0};
        VkResult vk_error = vkEnumeratePhysicalDevices(instance, &physical_devices_count, nullptr);
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to enumerate Vulkan physical devices. VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        if (physical_devices_count == 0) {
            out_error_message = "No Vulkan physical devices found.";
            return false;
        }

        std::vector<VkPhysicalDevice> physical_devices(physical_devices_count);
        vk_error = vkEnumeratePhysicalDevices(instance, &physical_devices_count, physical_devices.data());
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to enumerate Vulkan physical devices. VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        // Pick the highest-scoring suitable device.
        int best_score{0};
        for (VkPhysicalDevice candidate : physical_devices) {
            int score{rateDevice(candidate, surface)};
            if (score > best_score || (score >= 0 && m_physical_device == VK_NULL_HANDLE)) {
                best_score = score;
                m_physical_device = candidate;
            }
        }

        if (m_physical_device == VK_NULL_HANDLE) {
            out_error_message = "No supported Vulkan physical device found (need graphics + present queues and swapchain support).";
            return false;
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_physical_device, &properties);
        m_device_name = properties.deviceName;

        m_queue_families = findQueueFamilies(m_physical_device, surface);

        // Deduplicate queue family indices so we request each family once.
        std::set<uint32_t> unique_families{m_queue_families.graphics, m_queue_families.present};
        float queue_priority{1.0f};
        std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
        for (uint32_t family : unique_families) {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.pNext = nullptr;
            queue_create_info.flags = 0;
            queue_create_info.queueFamilyIndex = family;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queue_priority;
            queue_create_infos.push_back(queue_create_info);
        }

        VkPhysicalDeviceFeatures enabled_features{};

        VkDeviceCreateInfo device_create_info{};
        device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        device_create_info.pNext = nullptr;
        device_create_info.flags = 0;
        device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
        device_create_info.pQueueCreateInfos = queue_create_infos.data();
        device_create_info.enabledLayerCount = 0;
        device_create_info.ppEnabledLayerNames = nullptr;
        device_create_info.enabledExtensionCount = static_cast<uint32_t>(REQUIRED_DEVICE_EXTENSIONS.size());
        device_create_info.ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data();
        device_create_info.pEnabledFeatures = &enabled_features;

        vk_error = vkCreateDevice(m_physical_device, &device_create_info, nullptr, &m_vk_logical_device);
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to create Vulkan logical device. VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        volkLoadDevice(m_vk_logical_device);

        vkGetDeviceQueue(m_vk_logical_device, m_queue_families.graphics, 0, &m_graphics_queue);
        vkGetDeviceQueue(m_vk_logical_device, m_queue_families.present, 0, &m_present_queue);

        return true;
    }

    void Device::destroy()
    {
        if (m_vk_logical_device != VK_NULL_HANDLE) {
            vkDestroyDevice(m_vk_logical_device, nullptr);
            m_vk_logical_device = VK_NULL_HANDLE;
        }
        m_physical_device = VK_NULL_HANDLE;
        m_graphics_queue = VK_NULL_HANDLE;
        m_present_queue = VK_NULL_HANDLE;
    }

} // namespace Engine
