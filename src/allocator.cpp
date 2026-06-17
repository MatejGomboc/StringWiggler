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

#include "allocator.hpp"
#include <cstdlib>

namespace Engine
{

    Allocator::~Allocator()
    {
        destroy();
    }

    bool Allocator::init(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, LoggingLib::Logger& logger, std::string& out_error_message)
    {
        m_logger = &logger;

        VmaAllocatorCreateInfo alloc_info{};
        alloc_info.instance = instance;
        alloc_info.physicalDevice = physical_device;
        alloc_info.device = device;
        alloc_info.vulkanApiVersion = VK_API_VERSION_1_3;

        // Feed VMA the function pointers Volk already loaded (we build with VK_NO_PROTOTYPES,
        // so VMA must not assume static or its own dynamic linkage).
        VmaVulkanFunctions vma_functions{};
        VkResult result = vmaImportVulkanFunctionsFromVolk(&alloc_info, &vma_functions);
        if (result != VK_SUCCESS) {
            out_error_message = "Failed to import Vulkan functions from Volk for VMA. VK error:" + std::to_string(result) + ".";
            return false;
        }
        alloc_info.pVulkanFunctions = &vma_functions;

        result = vmaCreateAllocator(&alloc_info, &m_allocator);
        if (result != VK_SUCCESS) {
            out_error_message = "Failed to create VMA allocator. VK error:" + std::to_string(result) + ".";
            return false;
        }

        m_logger->logInfo("VMA allocator created.");
        return true;
    }

    void Allocator::destroy()
    {
        if (m_allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(m_allocator);
            m_allocator = VK_NULL_HANDLE;
            if (m_logger) {
                m_logger->logInfo("VMA allocator destroyed.");
            }
        }
    }

    AllocatedBuffer Allocator::createBuffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaAllocationCreateFlags alloc_flags, VmaMemoryUsage memory_usage) const
    {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = buffer_usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo alloc_create_info{};
        alloc_create_info.usage = memory_usage;
        alloc_create_info.flags = alloc_flags;

        VkBuffer buffer{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        VkResult result = vmaCreateBuffer(m_allocator, &buffer_info, &alloc_create_info, &buffer, &allocation, nullptr);
        if (result != VK_SUCCESS) {
            // Fail fast, die gracefully — an allocation failure here is unrecoverable.
            if (m_logger) {
                m_logger->logFatal("Failed to create VMA buffer. VK error:" + std::to_string(result) + ".");
            }
            std::abort();
        }

        return AllocatedBuffer(m_allocator, buffer, allocation);
    }

    AllocatedImage Allocator::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkSampleCountFlagBits sample_count,
        uint32_t mip_levels) const
    {
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = format;
        image_info.extent = {width, height, 1};
        image_info.mipLevels = mip_levels;
        image_info.arrayLayers = 1;
        image_info.samples = sample_count;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = usage;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo alloc_create_info{};
        alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        // Transient attachments benefit from lazily-allocated memory on tile-based GPUs.
        if (usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) {
            alloc_create_info.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
        }

        VkImage image{VK_NULL_HANDLE};
        VmaAllocation allocation{VK_NULL_HANDLE};
        VkResult result = vmaCreateImage(m_allocator, &image_info, &alloc_create_info, &image, &allocation, nullptr);
        if (result != VK_SUCCESS) {
            if (m_logger) {
                m_logger->logFatal("Failed to create VMA image. VK error:" + std::to_string(result) + ".");
            }
            std::abort();
        }

        return AllocatedImage(m_allocator, image, allocation);
    }

} // namespace Engine
