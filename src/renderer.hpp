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

#include "device.hpp"
#include "instance.hpp"
#include "native_window_handle.hpp"
#include <log/logger.hpp>
#include <volk/volk.h>
#include <string>

namespace Engine
{

    //! Composition root for the Vulkan back end. Owns, in dependency order, the instance,
    //! the window surface and the logical device. Subsequent milestones (swapchain,
    //! pipeline, frame synchronisation) will be added here.
    class Renderer {
    public:
        Renderer() = default;
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        //! Initialises the Vulkan back end for the given native window. Returns false and
        //! fills out_error_message on failure. The logger must outlive the renderer.
        [[nodiscard]] bool init(LoggingLib::Logger& logger, const NativeWindowHandle& window_handle, std::string& out_error_message);

        //! Tears down the back end in reverse construction order. Safe to call repeatedly.
        void destroy();

    private:
        Instance m_instance; //!< Vulkan instance + debug messenger.
        VkSurfaceKHR m_surface{VK_NULL_HANDLE}; //!< Window surface (owned here, between instance and device).
        Device m_device; //!< Physical + logical device and queues.
        bool m_initialised{false}; //!< True once init() has succeeded.
    };

} // namespace Engine
