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

#include <log/logger.hpp>
#ifdef _WIN32
#include <Volk/volk.h>
#else
#include <volk/volk.h>
#endif
#include <string>

namespace Engine
{

    //! Owns the VkInstance and (in debug builds) the validation debug messenger.
    //! First component constructed during renderer initialisation; last destroyed.
    class Instance {
    public:
        Instance() = default;
        ~Instance();

        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&) = delete;
        Instance& operator=(Instance&&) = delete;

        //! Initialises Volk, creates the Vulkan instance and (in debug builds) a validation
        //! messenger routed to the given logger. Returns false and fills out_error_message
        //! on failure. The logger must outlive this Instance (it backs the debug callback).
        [[nodiscard]] bool init(LoggingLib::Logger& logger, std::string& out_error_message);

        //! Destroys the messenger and instance. Safe to call repeatedly.
        void destroy();

        //! Returns the underlying VkInstance handle.
        [[nodiscard]] VkInstance get() const
        {
            return m_vk_instance;
        }

    private:
#ifdef DEBUG
        static constexpr const char* VK_LAYER_KHRONOS_VALIDATION_NAME = "VK_LAYER_KHRONOS_validation";
#endif

        VkInstance m_vk_instance{VK_NULL_HANDLE}; //!< Vulkan instance handle.
#ifdef DEBUG
        VkDebugUtilsMessengerEXT m_vk_debug_messenger{VK_NULL_HANDLE}; //!< Validation messenger (debug builds only).
#endif
    };

} // namespace Engine
