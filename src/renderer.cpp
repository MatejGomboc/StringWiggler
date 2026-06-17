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

        // Each sub-component catches vk::raii exceptions internally and returns bool; the
        // outer try is a final safety net so nothing escapes the renderer boundary.
        try {
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
            if (!m_device.init(m_instance, m_surface, out_error_message)) {
                destroy();
                return false;
            }
            logger.logInfo("Selected GPU \"" + m_device.name() + "\" for rendering.");

            // 4. VMA allocator (depends on instance + device).
            if (!m_allocator.init(m_instance.handle(), *m_device.physicalDevice(), *m_device.get(), logger, out_error_message)) {
                destroy();
                return false;
            }
        } catch (const vk::SystemError& e) {
            out_error_message = std::string("Vulkan error during renderer initialisation: ") + e.what();
            destroy();
            return false;
        } catch (const std::exception& e) {
            out_error_message = std::string("Error during renderer initialisation: ") + e.what();
            destroy();
            return false;
        }

        m_initialised = true;
        return true;
    }

    void Renderer::destroy()
    {
        // Reverse construction order: allocator, device, surface, instance.
        m_allocator.destroy();
        m_device.destroy();
        m_surface = nullptr;
        m_instance.destroy();
        m_initialised = false;
    }

} // namespace Engine
