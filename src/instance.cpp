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

#include "instance.hpp"
#include "surface.hpp"
#include "vulkan_helpers.hpp"
#include <string>
#include <vector>

// Storage for the vulkan-hpp dynamic dispatcher — exactly one translation unit.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Engine
{

#ifdef DEBUG
    //! Vulkan validation callback — routes layer messages to the logger by severity.
    //! pUserData is a LoggingLib::Logger* supplied at messenger-creation time.
    static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
    {
        auto* logger = static_cast<LoggingLib::Logger*>(user_data);
        if (!logger) {
            return vk::False;
        }

        std::string type_str = "[";
        if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance) {
            type_str += "PERFORMANCE,";
        }
        if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation) {
            type_str += "VALIDATION,";
        }
        if (type & vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral) {
            type_str += "GENERAL,";
        }
        if (type_str.size() > 1) {
            type_str.pop_back();
        }
        type_str += "]";

        std::string message = "[LAYER] " + type_str + " " + std::string(callback_data->pMessage);

        if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
            logger->logError(message);
        } else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
            logger->logWarning(message);
        } else if (severity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
            logger->logInfo(message);
        } else {
            logger->logDebug(message);
        }

        return vk::False;
    }
#endif

    Instance::~Instance()
    {
        destroy();
    }

    // logger is only referenced when DEBUG is defined (it backs the validation messenger),
    // so mark it [[maybe_unused]] to keep Release (no DEBUG) warning-clean under -Werror.
    bool Instance::init([[maybe_unused]] LoggingLib::Logger& logger, std::string& out_error_message)
    {
        // Step 1: Volk finds the Vulkan loader, then feed its vkGetInstanceProcAddr to the
        // vulkan-hpp default dispatcher (used by the free enumerate* functions below).
        if (volkInitialize() != VK_SUCCESS) {
            out_error_message = "Vulkan not found on this system.";
            return false;
        }
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        try {
            // Step 2: require Vulkan 1.3+.
            uint32_t api_version = vk::enumerateInstanceVersion();
            if ((VK_API_VERSION_VARIANT(api_version) != 0) || (VK_API_VERSION_MAJOR(api_version) != 1) || (VK_API_VERSION_MINOR(api_version) < 3)) {
                out_error_message = "Unsupported Vulkan version:" + std::to_string(VK_API_VERSION_MAJOR(api_version)) + "."
                    + std::to_string(VK_API_VERSION_MINOR(api_version)) + "." + std::to_string(VK_API_VERSION_PATCH(api_version));
                return false;
            }

            // Step 3: required instance extensions (surface WSI + debug utils in debug builds).
            std::vector<const char*> extensions = requiredSurfaceExtensions();
#ifdef DEBUG
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
            std::vector<vk::ExtensionProperties> available_extensions = vk::enumerateInstanceExtensionProperties();
            for (const char* extension : extensions) {
                if (!isExtensionAvailable(available_extensions, extension)) {
                    out_error_message = "Vulkan instance extension \"" + std::string(extension) + "\" not supported.";
                    return false;
                }
            }

            // Step 4: validation layer (debug builds only).
            std::vector<const char*> layers;
#ifdef DEBUG
            std::vector<vk::LayerProperties> available_layers = vk::enumerateInstanceLayerProperties();
            if (!isLayerAvailable(available_layers, "VK_LAYER_KHRONOS_validation")) {
                out_error_message = "Vulkan instance layer \"VK_LAYER_KHRONOS_validation\" not supported.";
                return false;
            }
            layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

            // Step 5: create the instance.
            vk::ApplicationInfo app_info{};
            app_info.setPApplicationName("StringWiggler");
            app_info.applicationVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
            app_info.setPEngineName("StringWiggler");
            app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 1, 0);
            app_info.apiVersion = VK_API_VERSION_1_3;

            vk::InstanceCreateInfo create_info{};
            create_info.setPApplicationInfo(&app_info);
            create_info.setPEnabledLayerNames(layers);
            create_info.setPEnabledExtensionNames(extensions);

#ifdef DEBUG
            // Chain the messenger into instance creation so creation/destruction is validated.
            vk::DebugUtilsMessengerCreateInfoEXT debug_create_info{};
            // Warning + Error only: Verbose/Info drown the console in loader/best-practice
            // chatter (hundreds of lines per run). Widen this when chasing a specific issue.
            debug_create_info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
            debug_create_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation
                | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
            debug_create_info.pfnUserCallback = vulkanDebugCallback;
            debug_create_info.setPUserData(&logger);
            create_info.setPNext(&debug_create_info);
#endif

            m_instance = vk::raii::Instance(m_context, create_info);

            // Step 6: load instance-level function pointers via Volk + the dispatcher.
            volkLoadInstanceOnly(*m_instance);
            VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_instance);

#ifdef DEBUG
            // Step 7: create the persistent debug messenger.
            m_debug_messenger = m_instance.createDebugUtilsMessengerEXT(debug_create_info);
#endif
        } catch (const vk::SystemError& e) {
            out_error_message = std::string("Vulkan error during instance creation: ") + e.what();
            return false;
        } catch (const std::exception& e) {
            out_error_message = std::string("Error during instance creation: ") + e.what();
            return false;
        }

        return true;
    }

    void Instance::destroy()
    {
        // Assigning nullptr to a vk::raii handle destroys it. Messenger before instance.
#ifdef DEBUG
        m_debug_messenger = nullptr;
#endif
        m_instance = nullptr;
    }

} // namespace Engine
