#pragma once

#include <volk/volk.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Engine {

struct NativeWindowHandle {
#if defined(_WIN32)
    void* hinstance; // HINSTANCE
    void* hwnd;      // HWND
#elif defined(__linux__) && defined(VK_USE_PLATFORM_XLIB_KHR)
    void* display;        // Display*
    unsigned long window; // Window (X11)
#elif defined(__linux__) && defined(VK_USE_PLATFORM_WAYLAND_KHR)
    void* display; // wl_display*
    void* surface; // wl_surface*
#elif defined(__APPLE__)
    void* layer; // CAMetalLayer*
#endif
};

class Renderer {
public:
    ~Renderer();
    bool init(std::string& out_error_message, const NativeWindowHandle& window_handle
#ifdef DEBUG
        ,
        PFN_vkDebugUtilsMessengerCallbackEXT vulkan_debug_callback, void* vulkan_debug_callback_user_data
#endif
    );
    void destroy();
    bool getSupportedPhysicalDevices(std::vector<VkPhysicalDevice>& out_supported_devices, std::string& out_error_message);
    bool createLogicalDevice(const VkPhysicalDevice& physical_device, std::string& out_error_message);

private:
    bool createSurface(const NativeWindowHandle& window_handle, std::string& out_error_message);
    static bool areDeviceExtensionsSupported(const VkPhysicalDevice& physical_device, const std::vector<const char*>& extensions, std::string& out_error_message);

#ifdef DEBUG
    static constexpr const char* const VK_LAYER_KHRONOS_VALIDATION_NAME = "VK_LAYER_KHRONOS_validation";
#endif

    bool m_initialized = false;
    VkInstance m_vk_instance = VK_NULL_HANDLE;
#ifdef DEBUG
    VkDebugUtilsMessengerEXT m_vk_debug_messenger = VK_NULL_HANDLE;
#endif
    VkSurfaceKHR m_vk_surface = VK_NULL_HANDLE;
    VkDevice m_vk_logical_device = VK_NULL_HANDLE;
};

}  // namespace Engine
