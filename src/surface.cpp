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

#include "surface.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <cstdint>
#include <xcb/xcb.h>
#endif

namespace Engine
{

    std::vector<const char*> requiredSurfaceExtensions()
    {
        return {
            VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_WIN32)
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
            VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
        };
    }

    bool createSurface(VkInstance instance, const NativeWindowHandle& window_handle, VkSurfaceKHR& out_surface, std::string& out_error_message)
    {
        VkResult vk_error;

#if defined(_WIN32)
        VkWin32SurfaceCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        create_info.pNext = nullptr;
        create_info.flags = 0;
        create_info.hinstance = static_cast<HINSTANCE>(window_handle.display);
        create_info.hwnd = static_cast<HWND>(window_handle.window);

        vk_error = vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &out_surface);

#elif defined(__linux__)
        VkXcbSurfaceCreateInfoKHR create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        create_info.pNext = nullptr;
        create_info.flags = 0;
        create_info.connection = static_cast<xcb_connection_t*>(window_handle.display);
        create_info.window = static_cast<xcb_window_t>(reinterpret_cast<uintptr_t>(window_handle.window));

        vk_error = vkCreateXcbSurfaceKHR(instance, &create_info, nullptr, &out_surface);
#else
        out_error_message = "Unsupported platform for Vulkan surface creation.";
        return false;
#endif

        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to create Vulkan rendering surface. VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        return true;
    }

} // namespace Engine
