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

#if defined(__linux__)
#include <cstdint>
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

    bool createSurface(const vk::raii::Instance& instance, const NativeWindowHandle& window_handle, vk::raii::SurfaceKHR& out_surface, std::string& out_error_message)
    {
        try {
#if defined(_WIN32)
            vk::Win32SurfaceCreateInfoKHR create_info{};
            create_info.hinstance = static_cast<HINSTANCE>(window_handle.display);
            create_info.hwnd = static_cast<HWND>(window_handle.window);
            out_surface = vk::raii::SurfaceKHR(instance, create_info);
#elif defined(__linux__)
            vk::XcbSurfaceCreateInfoKHR create_info{};
            create_info.connection = static_cast<xcb_connection_t*>(window_handle.display);
            create_info.window = static_cast<xcb_window_t>(reinterpret_cast<uintptr_t>(window_handle.window));
            out_surface = vk::raii::SurfaceKHR(instance, create_info);
#else
            out_error_message = "Unsupported platform for Vulkan surface creation.";
            return false;
#endif
        } catch (const vk::SystemError& e) {
            out_error_message = std::string("Failed to create Vulkan surface: ") + e.what();
            return false;
        } catch (const std::exception& e) {
            out_error_message = std::string("Error creating Vulkan surface: ") + e.what();
            return false;
        }

        return true;
    }

} // namespace Engine
