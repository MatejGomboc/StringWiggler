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

#include "renderer.hpp"
#include "surface.hpp"

namespace Engine
{

    Renderer::~Renderer()
    {
        destroy();
    }

    bool Renderer::init(LoggingLib::Logger& logger, const NativeWindowHandle& window_handle, std::string& out_error_message)
    {
        if (m_initialised) {
            out_error_message = "Renderer already initialised.";
            return false;
        }

        // 1. Instance (+ validation messenger in debug builds).
        if (!m_instance.init(logger, out_error_message)) {
            destroy();
            return false;
        }
        logger.logInfo("Vulkan instance created.");

        // 2. Surface for the native window.
        if (!createSurface(m_instance.get(), window_handle, m_surface, out_error_message)) {
            destroy();
            return false;
        }

        // 3. Physical + logical device and queues.
        if (!m_device.init(m_instance.get(), m_surface, out_error_message)) {
            destroy();
            return false;
        }
        logger.logInfo("Selected GPU \"" + m_device.name() + "\" for rendering.");

        m_initialised = true;
        return true;
    }

    void Renderer::destroy()
    {
        // Reverse construction order: device, then surface (while the instance is still
        // alive to destroy it), then the instance itself.
        m_device.destroy();

        if ((m_instance.get() != VK_NULL_HANDLE) && (m_surface != VK_NULL_HANDLE)) {
            vkDestroySurfaceKHR(m_instance.get(), m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }

        m_instance.destroy();
        m_initialised = false;
    }

} // namespace Engine
