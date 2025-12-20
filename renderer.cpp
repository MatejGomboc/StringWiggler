#include "renderer.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__) && defined(VK_USE_PLATFORM_XLIB_KHR)
#include <X11/Xlib.h>
#elif defined(__linux__) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <wayland-client.h>
#elif defined(__APPLE__)
// Metal types are forward-declared, no header needed here
#endif

using namespace Engine;

Renderer::~Renderer()
{
    destroy();
}

bool Renderer::createSurface(const NativeWindowHandle& window_handle, std::string& out_error_message)
{
    VkResult vk_error;

#if defined(_WIN32)
    VkWin32SurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.hinstance = static_cast<HINSTANCE>(window_handle.hinstance);
    create_info.hwnd = static_cast<HWND>(window_handle.hwnd);

    vk_error = vkCreateWin32SurfaceKHR(m_vk_instance, &create_info, nullptr, &m_vk_surface);

#elif defined(__linux__) && defined(VK_USE_PLATFORM_XLIB_KHR)
    VkXlibSurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.dpy = static_cast<Display*>(window_handle.display);
    create_info.window = static_cast<Window>(window_handle.window);

    vk_error = vkCreateXlibSurfaceKHR(m_vk_instance, &create_info, nullptr, &m_vk_surface);

#elif defined(__linux__) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VkWaylandSurfaceCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.display = static_cast<wl_display*>(window_handle.display);
    create_info.surface = static_cast<wl_surface*>(window_handle.surface);

    vk_error = vkCreateWaylandSurfaceKHR(m_vk_instance, &create_info, nullptr, &m_vk_surface);

#elif defined(__APPLE__)
    VkMetalSurfaceCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.pLayer = window_handle.layer;

    vk_error = vkCreateMetalSurfaceEXT(m_vk_instance, &create_info, nullptr, &m_vk_surface);
#endif

    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to create Vulkan rendering surface. VK error:" + std::to_string(vk_error) + ".";
        return false;
    }

    return true;
}

bool Renderer::init(std::string& out_error_message, const NativeWindowHandle& window_handle
#ifdef DEBUG
    ,
    PFN_vkDebugUtilsMessengerCallbackEXT vulkan_debug_callback, void* vulkan_debug_callback_user_data
#endif
)
{
    if (m_initialized) {
        out_error_message = "Renderer already initialized.";
        destroy();
        return false;
    }

    if (volkInitialize() != VK_SUCCESS) {
        out_error_message = "Vulkan not found on this system.";
        destroy();
        return false;
    }

    uint32_t vk_version = volkGetInstanceVersion();
    if (vk_version == 0) {
        out_error_message = "Failed to get Vulkan version.";
        destroy();
        return false;
    }

    if ((VK_API_VERSION_VARIANT(vk_version) != 0) || (VK_API_VERSION_MAJOR(vk_version) != 1) || (VK_API_VERSION_MINOR(vk_version) < 3)) {
        out_error_message = "Unsupported Vulkan version:" + std::to_string(VK_API_VERSION_MAJOR(vk_version)) + "." + std::to_string(VK_API_VERSION_MINOR(vk_version)) +
            "." + std::to_string(VK_API_VERSION_PATCH(vk_version)) + ":" + std::to_string(VK_API_VERSION_VARIANT(vk_version));
        destroy();
        return false;
    }

    /**************************************************************************************/

    VkResult vk_error;

#ifdef DEBUG
    uint32_t supported_instance_layers_count;
    vk_error = vkEnumerateInstanceLayerProperties(&supported_instance_layers_count, nullptr);
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to enumerate Vulkan instance layers. VK error:" + std::to_string(vk_error) + ".";
        destroy();
        return false;
    }

    if (supported_instance_layers_count == 0) {
        out_error_message = "No Vulkan instance layers found.";
        destroy();
        return false;
    }

    std::vector<VkLayerProperties> supported_instance_layers(supported_instance_layers_count);
    vk_error = vkEnumerateInstanceLayerProperties(&supported_instance_layers_count, supported_instance_layers.data());
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to enumerate Vulkan instance layers. VK error:" + std::to_string(vk_error) + ".";
        destroy();
        return false;
    }

    std::vector<const char*> instance_layers{VK_LAYER_KHRONOS_VALIDATION_NAME};

    for (const char* const instance_layer : instance_layers) {
        bool found = false;

        for (const VkLayerProperties& supported_instance_layer : supported_instance_layers) {
            if (std::string(instance_layer) == std::string(supported_instance_layer.layerName)) {
                found = true;
                break;
            }
        }

        if (!found) {
            out_error_message = "Vulkan instance layer \"" + std::string(instance_layer) + "\" not supported.";
            destroy();
            return false;
        }
    }
#endif

    /**************************************************************************************/

    std::vector<const char*> instance_extensions{VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(_WIN32)
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(__linux__) && defined(VK_USE_PLATFORM_XLIB_KHR)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#elif defined(__linux__) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#elif defined(__APPLE__)
        VK_EXT_METAL_SURFACE_EXTENSION_NAME, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
#endif
#ifdef DEBUG
        ,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
    };

    uint32_t supported_instance_extensions_count;
    vk_error = vkEnumerateInstanceExtensionProperties(nullptr, &supported_instance_extensions_count, nullptr);
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to enumerate Vulkan instance extensions. VK error:" + std::to_string(vk_error) + ".";
        destroy();
        return false;
    }

    if (supported_instance_extensions_count == 0) {
        out_error_message = "No Vulkan instance extensions found.";
        destroy();
        return false;
    }

    std::vector<VkExtensionProperties> supported_instance_extensions(supported_instance_extensions_count);
    vk_error = vkEnumerateInstanceExtensionProperties(nullptr, &supported_instance_extensions_count, supported_instance_extensions.data());
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to enumerate Vulkan instance extensions. VK error:" + std::to_string(vk_error) + ".";
        destroy();
        return false;
    }

    for (const char* const instance_extension : instance_extensions) {
        bool found = false;

        for (const VkExtensionProperties& supported_instance_extension : supported_instance_extensions) {
            if (std::string(instance_extension) == std::string(supported_instance_extension.extensionName)) {
                found = true;
                break;
            }
        }

        if (!found) {
            out_error_message = "Vulkan instance extension \"" + std::string(instance_extension) + "\" not supported.";
            destroy();
            return false;
        }
    }

    /**************************************************************************************/

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = nullptr;
    app_info.applicationVersion = 0;
    app_info.pEngineName = nullptr;
    app_info.engineVersion = 0;
    app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);

#ifdef DEBUG
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info{};
    debug_messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_info.pfnUserCallback = vulkan_debug_callback;
    debug_messenger_info.pUserData = vulkan_debug_callback_user_data;
#endif

    VkInstanceCreateInfo inst_info{};
    inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
#ifdef DEBUG
    inst_info.pNext = &debug_messenger_info;
#else
    inst_info.pNext = nullptr;
#endif
#if defined(__APPLE__)
    inst_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#else
    inst_info.flags = 0;
#endif
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
        destroy();
        return false;
    }

    volkLoadInstanceOnly(m_vk_instance);

    /**************************************************************************************/

#ifdef DEBUG
    vk_error = vkCreateDebugUtilsMessengerEXT(m_vk_instance, &debug_messenger_info, nullptr, &m_vk_debug_messenger);
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to create Vulkan debug messenger. VK error:" + std::to_string(vk_error) + ".";
        destroy();
        return false;
    }
#endif

    /**************************************************************************************/

    if (!createSurface(window_handle, out_error_message)) {
        destroy();
        return false;
    }

    m_initialized = true;
    return true;
}

void Renderer::destroy()
{
    if (m_vk_logical_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_vk_logical_device, nullptr);
        m_vk_logical_device = VK_NULL_HANDLE;
    }

    if ((m_vk_instance != VK_NULL_HANDLE) && (m_vk_surface != VK_NULL_HANDLE)) {
        vkDestroySurfaceKHR(m_vk_instance, m_vk_surface, nullptr);
        m_vk_surface = VK_NULL_HANDLE;
    }

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

    m_initialized = false;
}

bool Renderer::getSupportedPhysicalDevices(std::vector<VkPhysicalDevice>& out_supported_devices, std::string& out_error_message)
{
    if (!m_initialized) {
        out_error_message = "Renderer not initialized.";
        return false;
    }

    uint32_t physical_devices_count;
    VkResult vk_error = vkEnumeratePhysicalDevices(m_vk_instance, &physical_devices_count, nullptr);
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to enumerate Vulkan physical devices. VK error:" + std::to_string(vk_error) + ".";
        return false;
    }

    if (physical_devices_count == 0) {
        out_error_message = "No Vulkan physical devices found.";
        return false;
    }

    std::vector<VkPhysicalDevice> physical_devices(physical_devices_count);
    vk_error = vkEnumeratePhysicalDevices(m_vk_instance, &physical_devices_count, physical_devices.data());
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to enumerate Vulkan physical devices. VK error:" + std::to_string(vk_error) + ".";
        return false;
    }

    /**************************************************************************************/

    for (const VkPhysicalDevice& physical_device : physical_devices) {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);

        /**************************************************************************************/

        bool device_supported = true;

#ifdef DEBUG
        uint32_t supported_device_layers_count;
        vk_error = vkEnumerateDeviceLayerProperties(physical_device, &supported_device_layers_count, nullptr);
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to enumerate layers for Vulkan physical device: \"" + std::string(physical_device_properties.deviceName) +
                "\". VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        if (supported_device_layers_count == 0) {
            out_error_message = "No layers found for Vulkan physical device: \"" + std::string(physical_device_properties.deviceName) + "\".";
            return false;
        }

        std::vector<VkLayerProperties> supported_device_layers(supported_device_layers_count);
        vk_error = vkEnumerateDeviceLayerProperties(physical_device, &supported_device_layers_count, supported_device_layers.data());
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to enumerate layers for Vulkan physical device: \"" + std::string(physical_device_properties.deviceName) +
                "\". VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        std::vector<const char*> device_layers{VK_LAYER_KHRONOS_VALIDATION_NAME};

        for (const char* const device_layer : device_layers) {
            bool device_layer_found = false;

            for (const VkLayerProperties& supported_device_layer : supported_device_layers) {
                if (std::string(device_layer) == std::string(supported_device_layer.layerName)) {
                    device_layer_found = true;
                    break;
                }
            }

            if (!device_layer_found) {
                device_supported = false;
            }
        }

        if (!device_supported) {
            continue;
        }
#endif

        /**************************************************************************************/

        uint32_t queue_families_count;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, nullptr);

        if (queue_families_count == 0) {
            out_error_message = "No Vulkan queue families found for physical device \"" + std::string(physical_device_properties.deviceName) + "\".";
            return false;
        }

        std::vector<VkQueueFamilyProperties> queue_families_props(queue_families_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, queue_families_props.data());

        bool graphics_queue_family_found = false;
        bool present_queue_family_found = false;

        for (uint32_t i = 0; i < queue_families_props.size(); i++) {
            if (queue_families_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphics_queue_family_found = true;
            }

            VkBool32 presentation_supported = false;
            vk_error = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, m_vk_surface, &presentation_supported);
            if (vk_error != VK_SUCCESS) {
                out_error_message = "Failed to query Vulkan physical device \"" + std::string(physical_device_properties.deviceName) +
                    "\" for presentation support. VK error:" + std::to_string(vk_error) + ".";
                return false;
            }

            if (presentation_supported) {
                present_queue_family_found = true;
            }

            if (graphics_queue_family_found && present_queue_family_found) {
                break;
            }
        }

        device_supported = graphics_queue_family_found && present_queue_family_found;

        if (device_supported) {
            out_supported_devices.push_back(physical_device);
        }
    }

    /**************************************************************************************/

    if (out_supported_devices.empty()) {
        out_error_message = "No supported Vulkan physical devices found.";
        return false;
    }

    return true;
}

bool Renderer::createLogicalDevice(const VkPhysicalDevice& physical_device, std::string& out_error_message)
{
    if (!m_initialized) {
        out_error_message = "Renderer not initialized.";
        return false;
    }

    if (m_vk_logical_device != VK_NULL_HANDLE) {
        out_error_message = "Vulkan logical device already created.";
        return false;
    }

    VkPhysicalDeviceProperties physical_device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);

    /**************************************************************************************/

    VkResult vk_error;

#ifdef DEBUG
    std::vector<const char*> device_layers{VK_LAYER_KHRONOS_VALIDATION_NAME};

    uint32_t supported_device_layers_count;
    vk_error = vkEnumerateDeviceLayerProperties(physical_device, &supported_device_layers_count, nullptr);
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to enumerate layers for Vulkan physical device: \"" + std::string(physical_device_properties.deviceName) +
            "\". VK error:" + std::to_string(vk_error) + ".";
        return false;
    }

    if (supported_device_layers_count == 0) {
        out_error_message = "No layers found for Vulkan physical device: \"" + std::string(physical_device_properties.deviceName) + "\".";
        return false;
    }

    std::vector<VkLayerProperties> supported_device_layers(supported_device_layers_count);
    vk_error = vkEnumerateDeviceLayerProperties(physical_device, &supported_device_layers_count, supported_device_layers.data());
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to enumerate layers for Vulkan physical device: \"" + std::string(physical_device_properties.deviceName) +
            "\". VK error:" + std::to_string(vk_error) + ".";
        return false;
    }

    for (const char* const device_layer : device_layers) {
        bool layer_found = false;

        for (const VkLayerProperties& supported_device_layer : supported_device_layers) {
            if (std::string(device_layer) == std::string(supported_device_layer.layerName)) {
                layer_found = true;
                break;
            }
        }

        if (!layer_found) {
            out_error_message = "Layer \"" + std::string(device_layer) + "\" not supported for Vulkan physical device: \"" +
                std::string(physical_device_properties.deviceName) + "\".";
            return false;
        }
    }
#endif

    /**************************************************************************************/

    uint32_t queue_families_count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, nullptr);

    if (queue_families_count == 0) {
        out_error_message = "No Vulkan queue families found for physical device \"" + std::string(physical_device_properties.deviceName) + "\".";
        return false;
    }

    std::vector<VkQueueFamilyProperties> queue_families_props(queue_families_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, queue_families_props.data());

    bool graphics_queue_family_found = false;
    bool present_queue_family_found = false;

    uint32_t graphics_queue_family_idx = 0;
    uint32_t present_queue_family_idx = 0;

    for (uint32_t i = 0; i < queue_families_props.size(); i++) {
        if (queue_families_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_family_idx = i;
            graphics_queue_family_found = true;
        }

        VkBool32 presentation_supported = false;
        vk_error = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, m_vk_surface, &presentation_supported);
        if (vk_error != VK_SUCCESS) {
            out_error_message = "Failed to query Vulkan physical device \"" + std::string(physical_device_properties.deviceName) +
                "\" for presentation support. VK error:" + std::to_string(vk_error) + ".";
            return false;
        }

        if (presentation_supported) {
            present_queue_family_idx = i;
            present_queue_family_found = true;
        }

        if (graphics_queue_family_found && present_queue_family_found) {
            break;
        }
    }

    if (!(graphics_queue_family_found && present_queue_family_found)) {
        out_error_message = "No Vulkan supported queue families found for physical device \"" + std::string(physical_device_properties.deviceName) + "\".";
        return false;
    }

    /**************************************************************************************/

    float device_queue_priority = 1.0f;

    VkDeviceQueueCreateInfo device_queue_create_info{};
    device_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    device_queue_create_info.pNext = nullptr;
    device_queue_create_info.flags = 0;
    device_queue_create_info.queueFamilyIndex = graphics_queue_family_idx;
    device_queue_create_info.queueCount = 1;
    device_queue_create_info.pQueuePriorities = &device_queue_priority;

    std::vector<VkDeviceQueueCreateInfo> device_queue_create_infos{device_queue_create_info};
    if (present_queue_family_idx != graphics_queue_family_idx) {
        device_queue_create_info.queueFamilyIndex = present_queue_family_idx;
        device_queue_create_infos.push_back(device_queue_create_info);
    }

    VkPhysicalDeviceFeatures enabled_device_features{};

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = nullptr;
    device_create_info.flags = 0;
    device_create_info.queueCreateInfoCount = static_cast<uint32_t>(device_queue_create_infos.size());
    device_create_info.pQueueCreateInfos = device_queue_create_infos.data();
#ifdef DEBUG
    device_create_info.enabledLayerCount = static_cast<uint32_t>(device_layers.size());
    device_create_info.ppEnabledLayerNames = device_layers.data();
#else
    device_create_info.enabledLayerCount = 0;
    device_create_info.ppEnabledLayerNames = nullptr;
#endif
    device_create_info.enabledExtensionCount = 0;
    device_create_info.ppEnabledExtensionNames = nullptr;
    device_create_info.pEnabledFeatures = &enabled_device_features;

    vk_error = vkCreateDevice(physical_device, &device_create_info, nullptr, &m_vk_logical_device);
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to create Vulkan logical device. VK error:" + std::to_string(vk_error) + ".";
        destroy();
        return false;
    }

    volkLoadDevice(m_vk_logical_device);

    return true;
}

bool Renderer::areDeviceExtensionsSupported(const VkPhysicalDevice& physical_device, const std::vector<const char*>& extensions, std::string& out_error_message)
{
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(physical_device, &device_properties);

    uint32_t supported_extensions_count;
    VkResult vk_error = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &supported_extensions_count, nullptr);
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to enumerate extensions for Vulkan physical device: \"" + std::string(device_properties.deviceName) +
            "\". VK error:" + std::to_string(vk_error) + ".";
        return false;
    }

    if (supported_extensions_count == 0) {
        out_error_message = "No device extensions found for for Vulkan physical device: \"" + std::string(device_properties.deviceName) + "\".";
        return false;
    }

    std::vector<VkExtensionProperties> supported_extensions(supported_extensions_count);
    vk_error = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &supported_extensions_count, supported_extensions.data());
    if (vk_error != VK_SUCCESS) {
        out_error_message = "Failed to enumerate extensions for Vulkan physical device: \"" + std::string(device_properties.deviceName) +
            "\". VK error:" + std::to_string(vk_error) + ".";
        return false;
    }

    for (const char* const extension : extensions) {
        bool found = false;

        for (const VkExtensionProperties& supported_extension : supported_extensions) {
            if (std::string(extension) == std::string(supported_extension.extensionName)) {
                found = true;
                break;
            }
        }

        if (!found) {
            out_error_message = "Extension \"" + std::string(extension) + "\" not supported for Vulkan physical device: \"" + std::string(device_properties.deviceName) +
                "\".";
            return false;
        }
    }

    return true;
}
