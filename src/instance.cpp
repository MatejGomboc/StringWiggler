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

namespace Engine
{

#ifdef DEBUG
    //! Vulkan validation callback — routes layer messages to the logger by severity.
    //! pUserData is a LoggingLib::Logger* supplied at messenger-creation time.
    static VkBool32 VKAPI_PTR vulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
    {
        std::string type_str = "[";
        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
            type_str += "PERFORMANCE,";
        }
        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
            type_str += "VALIDATION,";
        }
        if (message_type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
            type_str += "GENERAL,";
        }
        if (type_str.size() > 1) {
            type_str.pop_back();
        }
        type_str += "]";

        auto* logger = static_cast<LoggingLib::Logger*>(user_data);
        std::string message = "[LAYER] " + type_str + " " + std::string(callback_data->pMessage);

        if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            logger->logError(message);
        } else if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            logger->logWarning(message);
        } else if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            logger->logInfo(message);
        } else {
            logger->logDebug(message);
        }

        return VK_FALSE;
    }
#endif

    Instance::~Instance()
    {
        destroy();
    }

    bool Instance::init(LoggingLib::Logger& logger, std::string& out_error_message)
    {
        if (m_vk_instance != VK_NULL_HANDLE) {
            out_error_message = "Vulkan instance already initialised.";
            return false;
        }

        if (volkInitialize() != VK_SUCCESS) {
            out_error_message = "Vulkan not found on this system.";
            return false;
        }

        uint32_t vk_version = volkGetInstanceVersion();
        if (vk_version == 0) {
            out_error_message = "Failed to get Vulkan version.";
            return false;
        }

        if ((VK_API_VERSION_VARIANT(vk_version) != 0) || (VK_API_VERSION_MAJOR(vk_version) != 1) || (VK_API_VERSION_MINOR(vk_version) < 3)) {
            out_error_message = "Unsupported Vulkan version:" + std::to_string(VK_API_VERSION_MAJOR(vk_version)) + "." + std::to_string(VK_API_VERSION_MINOR(vk_version))
                + "." + std::to_string(VK_API_VERSION_PATCH(vk_version)) + ":" + std::to_string(VK_API_VERSION_VARIANT(vk_version));
            return false;
        }

        VkResult vk_error;

        // ---------------------------------------------------------------------------
        // Validation layer (debug builds only).
        // ---------------------------------------------------------------------------
#ifdef DEBUG
        uint32_t supported_instance_layers_count;
        vk_error = vkEnumerateInstanceLayerProperties(&supported_instance_layers_count, nullptr);
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to enumerate Vulkan instance layers. VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        std::vector<VkLayerProperties> supported_instance_layers(supported_instance_layers_count);
        vk_error = vkEnumerateInstanceLayerProperties(&supported_instance_layers_count, supported_instance_layers.data());
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to enumerate Vulkan instance layers. VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        std::vector<const char*> instance_layers{VK_LAYER_KHRONOS_VALIDATION_NAME};
        for (const char* const layer : instance_layers) {
            if (!isLayerAvailable(supported_instance_layers, layer)) {
                out_error_message = "Vulkan instance layer \"" + std::string(layer) + "\" not supported.";
                return false;
            }
        }
#endif

        // ---------------------------------------------------------------------------
        // Required instance extensions: surface (WSI) plus debug utils in debug builds.
        // ---------------------------------------------------------------------------
        std::vector<const char*> instance_extensions = requiredSurfaceExtensions();
#ifdef DEBUG
        instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

        uint32_t supported_instance_extensions_count;
        vk_error = vkEnumerateInstanceExtensionProperties(nullptr, &supported_instance_extensions_count, nullptr);
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to enumerate Vulkan instance extensions. VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        std::vector<VkExtensionProperties> supported_instance_extensions(supported_instance_extensions_count);
        vk_error = vkEnumerateInstanceExtensionProperties(nullptr, &supported_instance_extensions_count, supported_instance_extensions.data());
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to enumerate Vulkan instance extensions. VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        for (const char* const extension : instance_extensions) {
            if (!isExtensionAvailable(supported_instance_extensions, extension)) {
                out_error_message = "Vulkan instance extension \"" + std::string(extension) + "\" not supported.";
                return false;
            }
        }

        // ---------------------------------------------------------------------------
        // Instance creation.
        // ---------------------------------------------------------------------------
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pNext = nullptr;
        app_info.pApplicationName = "StringWiggler";
        app_info.applicationVersion = 0;
        app_info.pEngineName = nullptr;
        app_info.engineVersion = 0;
        app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);

#ifdef DEBUG
        VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info{};
        debug_messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_messenger_info.pfnUserCallback = vulkanDebugCallback;
        debug_messenger_info.pUserData = &logger;
#endif

        VkInstanceCreateInfo inst_info{};
        inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        // Chain the messenger into pNext so instance creation/destruction is itself validated.
#ifdef DEBUG
        inst_info.pNext = &debug_messenger_info;
#else
        inst_info.pNext = nullptr;
#endif
        inst_info.flags = 0;
        inst_info.pApplicationInfo = &app_info;
#ifdef DEBUG
        inst_info.enabledLayerCount = static_cast<uint32_t>(instance_layers.size());
        inst_info.ppEnabledLayerNames = instance_layers.data();
#else
        inst_info.enabledLayerCount = 0;
        inst_info.ppEnabledLayerNames = nullptr;
#endif
        inst_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
        inst_info.ppEnabledExtensionNames = instance_extensions.data();

        vk_error = vkCreateInstance(&inst_info, nullptr, &m_vk_instance);
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to create Vulkan instance. VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        volkLoadInstanceOnly(m_vk_instance);

#ifdef DEBUG
        vk_error = vkCreateDebugUtilsMessengerEXT(m_vk_instance, &debug_messenger_info, nullptr, &m_vk_debug_messenger);
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to create Vulkan debug messenger. VK error:" + std::to_string(vk_error) + ".";
            destroy();
            return false;
        }
#endif

        return true;
    }

    void Instance::destroy()
    {
#ifdef DEBUG
        if ((m_vk_instance != VK_NULL_HANDLE) && (m_vk_debug_messenger != VK_NULL_HANDLE)) {
            vkDestroyDebugUtilsMessengerEXT(m_vk_instance, m_vk_debug_messenger, nullptr);
            m_vk_debug_messenger = VK_NULL_HANDLE;
        }
#endif

        if (m_vk_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_vk_instance, nullptr);
            m_vk_instance = VK_NULL_HANDLE;
        }
    }

} // namespace Engine
