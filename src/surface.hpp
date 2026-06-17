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

#include "native_window_handle.hpp"
#ifdef _WIN32
#include <Volk/volk.h>
#else
#include <volk/volk.h>
#endif
#include <vulkan/vulkan_raii.hpp>
#include <string>
#include <vector>

namespace Engine
{

    //! Returns the instance-level surface extensions required for the current platform
    //! (VK_KHR_surface plus the platform-specific WSI extension). Colocated with
    //! createSurface() so all windowing-platform knowledge lives in one place.
    [[nodiscard]] std::vector<const char*> requiredSurfaceExtensions();

    //! Creates a Vulkan surface for the given native window. Returns false and fills
    //! out_error_message on failure. vk::raii exceptions are caught here, not propagated.
    [[nodiscard]] bool createSurface(const vk::raii::Instance& instance, const NativeWindowHandle& window_handle, vk::raii::SurfaceKHR& out_surface,
        std::string& out_error_message);

} // namespace Engine
