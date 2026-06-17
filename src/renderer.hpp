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

#include "allocator.hpp"
#include "device.hpp"
#include "instance.hpp"
#include "native_window_handle.hpp"
#include <log/logger.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <string>

namespace Engine
{

    //! Composition root for the Vulkan back end. Owns, in dependency order, the instance,
    //! the window surface, the logical device and the VMA allocator. Member declaration order
    //! gives correct reverse-order destruction (allocator, device, surface, instance).
    //!
    //! Exception policy: the underlying vk::raii types throw on Vulkan errors; those are
    //! caught at the init() boundary (here and in each sub-component) and translated to
    //! out_error_message, so no exception escapes the renderer's public API.
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
        vk::raii::SurfaceKHR m_surface{nullptr}; //!< Window surface (between instance and device).
        Device m_device; //!< Physical + logical device and queues.
        Allocator m_allocator; //!< VMA allocator (created after the device).
        bool m_initialised{false}; //!< True once init() has succeeded.
    };

} // namespace Engine
