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

#ifdef _WIN32
#include <Volk/volk.h>
#else
#include <volk/volk.h>
#endif

// Suppress warnings from the third-party VMA header — we cannot modify it.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <vma/vk_mem_alloc.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <log/logger.hpp>
#include <cstdint>
#include <string>

namespace Engine
{

    //! RAII wrapper for a VMA buffer and its allocation (move-only).
    class AllocatedBuffer {
    public:
        //! Takes ownership of a VMA buffer + allocation pair.
        AllocatedBuffer(VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation) :
            m_allocator(allocator),
            m_buffer(buffer),
            m_allocation(allocation)
        {
        }

        ~AllocatedBuffer()
        {
            if (m_buffer != VK_NULL_HANDLE) {
                vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
            }
        }

        AllocatedBuffer(const AllocatedBuffer&) = delete;
        AllocatedBuffer& operator=(const AllocatedBuffer&) = delete;

        AllocatedBuffer(AllocatedBuffer&& other) noexcept :
            m_allocator(other.m_allocator),
            m_buffer(other.m_buffer),
            m_allocation(other.m_allocation)
        {
            other.m_allocator = VK_NULL_HANDLE;
            other.m_buffer = VK_NULL_HANDLE;
            other.m_allocation = VK_NULL_HANDLE;
        }

        AllocatedBuffer& operator=(AllocatedBuffer&& other) noexcept
        {
            if (this != &other) {
                if (m_buffer != VK_NULL_HANDLE) {
                    vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
                }
                m_allocator = other.m_allocator;
                m_buffer = other.m_buffer;
                m_allocation = other.m_allocation;
                other.m_allocator = VK_NULL_HANDLE;
                other.m_buffer = VK_NULL_HANDLE;
                other.m_allocation = VK_NULL_HANDLE;
            }
            return *this;
        }

        //! Raw VkBuffer handle.
        [[nodiscard]] VkBuffer buffer() const
        {
            return m_buffer;
        }

        //! VMA allocation handle (for mapping, etc.).
        [[nodiscard]] VmaAllocation allocation() const
        {
            return m_allocation;
        }

        //! VMA allocation info (contains pMappedData for persistently mapped buffers).
        [[nodiscard]] VmaAllocationInfo allocationInfo() const
        {
            VmaAllocationInfo info{};
            vmaGetAllocationInfo(m_allocator, m_allocation, &info);
            return info;
        }

    private:
        VmaAllocator m_allocator{VK_NULL_HANDLE}; //!< Non-owning reference to the parent allocator.
        VkBuffer m_buffer{VK_NULL_HANDLE}; //!< Owned buffer handle.
        VmaAllocation m_allocation{VK_NULL_HANDLE}; //!< Owned allocation handle.
    };

    //! RAII wrapper for a VMA image and its allocation (move-only).
    class AllocatedImage {
    public:
        //! Takes ownership of a VMA image + allocation pair.
        AllocatedImage(VmaAllocator allocator, VkImage image, VmaAllocation allocation) :
            m_allocator(allocator),
            m_image(image),
            m_allocation(allocation)
        {
        }

        ~AllocatedImage()
        {
            if (m_image != VK_NULL_HANDLE) {
                vmaDestroyImage(m_allocator, m_image, m_allocation);
            }
        }

        AllocatedImage(const AllocatedImage&) = delete;
        AllocatedImage& operator=(const AllocatedImage&) = delete;

        AllocatedImage(AllocatedImage&& other) noexcept :
            m_allocator(other.m_allocator),
            m_image(other.m_image),
            m_allocation(other.m_allocation)
        {
            other.m_allocator = VK_NULL_HANDLE;
            other.m_image = VK_NULL_HANDLE;
            other.m_allocation = VK_NULL_HANDLE;
        }

        AllocatedImage& operator=(AllocatedImage&& other) noexcept
        {
            if (this != &other) {
                if (m_image != VK_NULL_HANDLE) {
                    vmaDestroyImage(m_allocator, m_image, m_allocation);
                }
                m_allocator = other.m_allocator;
                m_image = other.m_image;
                m_allocation = other.m_allocation;
                other.m_allocator = VK_NULL_HANDLE;
                other.m_image = VK_NULL_HANDLE;
                other.m_allocation = VK_NULL_HANDLE;
            }
            return *this;
        }

        //! Raw VkImage handle.
        [[nodiscard]] VkImage image() const
        {
            return m_image;
        }

    private:
        VmaAllocator m_allocator{VK_NULL_HANDLE}; //!< Non-owning reference to the parent allocator.
        VkImage m_image{VK_NULL_HANDLE}; //!< Owned image handle.
        VmaAllocation m_allocation{VK_NULL_HANDLE}; //!< Owned allocation handle.
    };

    //! Owns the VmaAllocator. Built from Volk function pointers after the device exists.
    class Allocator {
    public:
        Allocator() = default;
        ~Allocator();

        Allocator(const Allocator&) = delete;
        Allocator& operator=(const Allocator&) = delete;
        Allocator(Allocator&&) = delete;
        Allocator& operator=(Allocator&&) = delete;

        //! Creates the VMA allocator using Volk function pointers. Returns false and fills
        //! out_error_message on failure. The logger must outlive this Allocator.
        [[nodiscard]] bool init(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device, LoggingLib::Logger& logger, std::string& out_error_message);

        //! Destroys the allocator. Safe to call repeatedly.
        void destroy();

        //! Creates a GPU buffer with a VMA allocation (fatal on failure).
        [[nodiscard]] AllocatedBuffer createBuffer(VkDeviceSize size, VkBufferUsageFlags buffer_usage, VmaAllocationCreateFlags alloc_flags,
            VmaMemoryUsage memory_usage) const;

        //! Creates a 2D GPU image with a VMA allocation (fatal on failure).
        [[nodiscard]] AllocatedImage createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage,
            VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT, uint32_t mip_levels = 1) const;

        //! Raw VmaAllocator handle.
        [[nodiscard]] VmaAllocator handle() const
        {
            return m_allocator;
        }

    private:
        LoggingLib::Logger* m_logger{nullptr}; //!< Logger reference (non-owning), set in init().
        VmaAllocator m_allocator{VK_NULL_HANDLE}; //!< Owned VMA allocator handle.
    };

} // namespace Engine
