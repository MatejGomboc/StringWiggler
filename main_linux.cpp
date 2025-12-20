#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cstdlib>
#include <cstring>

#include "logger.h"
#include "renderer.h"

struct AppData {
    Engine::Logger logger;
    Engine::Renderer renderer;
    Display* display = nullptr;
    Window window = 0;
    Atom wm_delete_window;
    bool running = true;
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

    auto logger = static_cast<Engine::Logger*>(user_data);

    logger->logWrite("[LAYER] " + severity_str + " " + type_str + " " + std::string(callback_data->pMessage));

    return VK_FALSE;
}
#endif

static bool createWindow(AppData& app, int width, int height, const char* title)
{
    app.display = XOpenDisplay(nullptr);
    if (app.display == nullptr) {
        app.logger.logWrite("[ERROR] Failed to open X11 display.");
        return false;
    }

    int screen = DefaultScreen(app.display);
    Window root = RootWindow(app.display, screen);

    XSetWindowAttributes window_attrs{};
    window_attrs.background_pixel = BlackPixel(app.display, screen);
    window_attrs.border_pixel = BlackPixel(app.display, screen);
    window_attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;

    app.window = XCreateWindow(app.display, root, 0, 0, width, height, 0, CopyFromParent, InputOutput, CopyFromParent,
        CWBackPixel | CWBorderPixel | CWEventMask, &window_attrs);

    if (app.window == 0) {
        app.logger.logWrite("[ERROR] Failed to create X11 window.");
        XCloseDisplay(app.display);
        app.display = nullptr;
        return false;
    }

    XStoreName(app.display, app.window, title);

    app.wm_delete_window = XInternAtom(app.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(app.display, app.window, &app.wm_delete_window, 1);

    XMapWindow(app.display, app.window);
    XFlush(app.display);

    return true;
}

static void destroyWindow(AppData& app)
{
    if (app.window != 0) {
        XDestroyWindow(app.display, app.window);
        app.window = 0;
    }

    if (app.display != nullptr) {
        XCloseDisplay(app.display);
        app.display = nullptr;
    }
}

static void processEvents(AppData& app)
{
    while (XPending(app.display) > 0) {
        XEvent event;
        XNextEvent(app.display, &event);

        switch (event.type) {
        case ClientMessage:
            if (static_cast<Atom>(event.xclient.data.l[0]) == app.wm_delete_window) {
                app.running = false;
            }
            break;

        case DestroyNotify:
            app.running = false;
            break;

        case ConfigureNotify:
            // Window resize event - could handle swapchain recreation here
            break;

        default:
            break;
        }
    }
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    AppData app;

    std::string out_error_message;
    if (!app.logger.start("log.txt", out_error_message)) {
        return EXIT_FAILURE;
    }

    if (!createWindow(app, 800, 600, "StringWiggler")) {
        return EXIT_FAILURE;
    }

    Engine::NativeWindowHandle window_handle{};
    window_handle.display = app.display;
    window_handle.window = app.window;

#ifdef DEBUG
    if (!app.renderer.init(out_error_message, window_handle, vulkanDebugCallback, &app.logger)) {
#else
    if (!app.renderer.init(out_error_message, window_handle)) {
#endif
        app.logger.logWrite("[ERROR] " + out_error_message);
        destroyWindow(app);
        return EXIT_FAILURE;
    }

    std::vector<VkPhysicalDevice> supported_devices;
    if (!app.renderer.getSupportedPhysicalDevices(supported_devices, out_error_message)) {
        app.logger.logWrite("[ERROR] " + out_error_message);
        app.renderer.destroy();
        destroyWindow(app);
        return EXIT_FAILURE;
    }

    app.logger.logWrite("[INFO] Found supported Vulkan physical devices:");
    for (const VkPhysicalDevice& device : supported_devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        app.logger.logWrite("[INFO] \"" + std::string(props.deviceName) + "\".");
    }

    if (!app.renderer.createLogicalDevice(supported_devices[0], out_error_message)) {
        app.logger.logWrite("[ERROR] " + out_error_message);
        app.renderer.destroy();
        destroyWindow(app);
        return EXIT_FAILURE;
    }

    VkPhysicalDeviceProperties selected_props;
    vkGetPhysicalDeviceProperties(supported_devices[0], &selected_props);
    app.logger.logWrite("[INFO] Selected \"" + std::string(selected_props.deviceName) + "\" for rendering.");

    while (app.running) {
        processEvents(app);
        // Render frame here
    }

    app.renderer.destroy();
    destroyWindow(app);

    return EXIT_SUCCESS;
}
