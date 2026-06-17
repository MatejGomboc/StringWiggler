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
#include <vulkan/vulkan_raii.hpp>
#include <log/logger.hpp>
#include <string>

namespace Engine
{

    //! Owns the Vulkan instance and (in debug builds) the validation debug messenger.
    //! Cleanup is handled by vk::raii destructors (and explicit destroy() for ordered teardown).
    //!
    //! Exception policy: vk::raii throws on Vulkan errors; init() catches those at this
    //! boundary and translates them to out_error_message, so no exception escapes our API.
    class Instance {
    public:
        Instance() = default;
        ~Instance();

        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&) = delete;
        Instance& operator=(Instance&&) = delete;

        //! Initialises Volk + the vulkan-hpp dispatcher, then creates the instance and (in
        //! debug builds) a validation messenger routed to the logger. Returns false and fills
        //! out_error_message on failure. The logger must outlive this Instance.
        [[nodiscard]] bool init(LoggingLib::Logger& logger, std::string& out_error_message);

        //! Destroys the messenger and instance. Safe to call repeatedly.
        void destroy();

        //! Raw VkInstance handle (for VMA and surface creation).
        [[nodiscard]] VkInstance handle() const
        {
            return *m_instance;
        }

        //! RAII instance reference (for surface / device creation).
        [[nodiscard]] const vk::raii::Instance& get() const
        {
            return m_instance;
        }

    private:
        vk::raii::Context m_context; //!< Vulkan loader bootstrap.
        vk::raii::Instance m_instance{nullptr}; //!< Vulkan instance handle.
#ifdef DEBUG
        vk::raii::DebugUtilsMessengerEXT m_debug_messenger{nullptr}; //!< Validation messenger (debug builds only).
#endif
    };

} // namespace Engine
