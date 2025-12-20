#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "logger.h"
#include "renderer.h"

struct MainWindowUserData {
    Simulator::Logger logger;
    Simulator::Renderer renderer;
};

#ifdef DEBUG
static VkBool32 VKAPI_PTR vulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    std::string severity_str;
    if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severity_str = "[ERROR]";
    } else if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severity_str = "[WARNING]";
    } else if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        severity_str = "[INFO]";
    } else if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        severity_str = "[VERBOSE]";
    } else {
        severity_str = "[UNKNOWN]";
    }

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

    auto logger = static_cast<Simulator::Logger*>(user_data);

    logger->logWrite("[LAYER] " + severity_str + " " + type_str + " " + std::string(callback_data->pMessage));

    return VK_FALSE;
}
#endif

static LRESULT CALLBACK wndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        auto create_info = reinterpret_cast<CREATESTRUCTA*>(lparam);
        auto user_data = static_cast<MainWindowUserData*>(create_info->lpCreateParams);

        SetLastError(0);
        if (SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(user_data)) != 0) {
            user_data->logger.logWrite("[ERROR] Failed to store main window data.");
            return -1;
        }

        if (GetLastError() != ERROR_SUCCESS) {
            user_data->logger.logWrite("[ERROR] Failed to store main window data. Windows error:" + std::to_string(GetLastError()));
            return -1;
        }

        std::string out_error_message;
#ifdef DEBUG
        if (!user_data->renderer.init(out_error_message, create_info->hInstance, window, vulkanDebugCallback, &(user_data->logger))) {
#else
        if (!user_data->renderer.init(out_error_message, create_info->hInstance, window)) {
#endif
            user_data->logger.logWrite("[ERROR] " + out_error_message);
            return -1;
        }

        std::vector<VkPhysicalDevice> out_supported_vk_physical_devices;
        if (!user_data->renderer.getSupportedPhysicalDevices(out_supported_vk_physical_devices, out_error_message)) {
            user_data->logger.logWrite("[ERROR] " + out_error_message);
            return -1;
        }

        user_data->logger.logWrite("[INFO] Found supported Vulkan physical devices:");
        for (const VkPhysicalDevice& vk_physical_device : out_supported_vk_physical_devices) {
            VkPhysicalDeviceProperties vk_physical_device_properties;
            vkGetPhysicalDeviceProperties(vk_physical_device, &vk_physical_device_properties);
            user_data->logger.logWrite("[INFO] \"" + std::string(vk_physical_device_properties.deviceName) + "\".");
        }

        if (!user_data->renderer.createLogicalDevice(out_supported_vk_physical_devices[0], out_error_message)) {
            user_data->logger.logWrite("[ERROR] " + out_error_message);
            return -1;
        }

        VkPhysicalDeviceProperties vk_physical_device_properties;
        vkGetPhysicalDeviceProperties(out_supported_vk_physical_devices[0], &vk_physical_device_properties);
        user_data->logger.logWrite("[INFO] Selected \"" + std::string(vk_physical_device_properties.deviceName) + "\" for rendering.");

        return 0;
    }
    case WM_DESTROY: {
        auto user_data = reinterpret_cast<MainWindowUserData*>(GetWindowLongPtr(window, GWLP_USERDATA));
        if (user_data == nullptr) {
            PostQuitMessage(GetLastError());
            return 0;
        }

        user_data->renderer.destroy();
        PostQuitMessage(ERROR_SUCCESS);
        return 0;
    }
    default:
        return DefWindowProc(window, message, wparam, lparam);
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE app_instance, _In_opt_ HINSTANCE prev_app_instance, _In_ LPWSTR cmd_line, _In_ int cmd_show)
{
    UNREFERENCED_PARAMETER(prev_app_instance);
    UNREFERENCED_PARAMETER(cmd_line);

    MainWindowUserData main_window_user_data;

    std::string out_error_message;
    if (!main_window_user_data.logger.start("log.txt", out_error_message)) {
        return -1;
    }

    WNDCLASSEX main_window_class{};
    main_window_class.cbSize = sizeof(WNDCLASSEX);
    main_window_class.style = CS_HREDRAW | CS_VREDRAW;
    main_window_class.lpfnWndProc = wndProc;
    main_window_class.cbClsExtra = 0;
    main_window_class.cbWndExtra = sizeof(LONG_PTR);
    main_window_class.hInstance = app_instance;
    main_window_class.hIcon = nullptr;
    main_window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    main_window_class.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    main_window_class.lpszMenuName = nullptr;
    main_window_class.lpszClassName = TEXT("MainWindow");
    main_window_class.hIconSm = nullptr;

    if (RegisterClassEx(&main_window_class) == 0) {
        main_window_user_data.logger.logWrite("[ERROR] Failed to register main window class. Windows error:" + std::to_string(GetLastError()));
        return GetLastError();
    }

    HWND main_window = CreateWindow(TEXT("MainWindow"), TEXT("Simulator"), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr,
        nullptr, app_instance, &main_window_user_data);

    if (main_window == nullptr) {
        main_window_user_data.logger.logWrite("[ERROR] Failed to create main window. Windows error:" + std::to_string(GetLastError()));
        return GetLastError();
    }

    ShowWindow(main_window, cmd_show);

    MSG message;
    while (GetMessage(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return (int)message.wParam;
}
