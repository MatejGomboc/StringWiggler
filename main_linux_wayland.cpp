#include <wayland-client.h>
#include <cstdlib>
#include <cstring>

#include "logger.h"
#include "renderer.h"

// Generated from xdg-shell.xml by wayland-scanner
#include "xdg-shell-client-protocol.h"

struct AppData {
    Engine::Logger logger;
    Engine::Renderer renderer;

    // Wayland core objects
    wl_display* display = nullptr;
    wl_registry* registry = nullptr;
    wl_compositor* compositor = nullptr;
    wl_surface* surface = nullptr;

    // XDG shell objects
    xdg_wm_base* xdg_wm_base = nullptr;
    xdg_surface* xdg_surface = nullptr;
    xdg_toplevel* xdg_toplevel = nullptr;

    bool running = true;
    bool configured = false;
    int width = 800;
    int height = 600;
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

// XDG WM Base listener
static void xdgWmBasePing(void* data, xdg_wm_base* xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdgWmBasePing,
};

// XDG Surface listener
static void xdgSurfaceConfigure(void* data, xdg_surface* xdg_surface, uint32_t serial)
{
    auto app = static_cast<AppData*>(data);
    xdg_surface_ack_configure(xdg_surface, serial);
    app->configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdgSurfaceConfigure,
};

// XDG Toplevel listener
static void xdgToplevelConfigure(void* data, xdg_toplevel* xdg_toplevel, int32_t width, int32_t height, wl_array* states)
{
    auto app = static_cast<AppData*>(data);
    if (width > 0 && height > 0) {
        app->width = width;
        app->height = height;
    }
}

static void xdgToplevelClose(void* data, xdg_toplevel* xdg_toplevel)
{
    auto app = static_cast<AppData*>(data);
    app->running = false;
}

static void xdgToplevelConfigureBounds(void* data, xdg_toplevel* xdg_toplevel, int32_t width, int32_t height)
{
    // Optional: handle bounds
}

static void xdgToplevelWmCapabilities(void* data, xdg_toplevel* xdg_toplevel, wl_array* capabilities)
{
    // Optional: handle WM capabilities
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdgToplevelConfigure,
    .close = xdgToplevelClose,
    .configure_bounds = xdgToplevelConfigureBounds,
    .wm_capabilities = xdgToplevelWmCapabilities,
};

// Registry listener
static void registryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
{
    auto app = static_cast<AppData*>(data);

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        app->compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, 4));
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        app->xdg_wm_base = static_cast<struct xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        xdg_wm_base_add_listener(app->xdg_wm_base, &xdg_wm_base_listener, app);
    }
}

static void registryGlobalRemove(void* data, wl_registry* registry, uint32_t name)
{
    // Handle global removal if needed
}

static const struct wl_registry_listener registry_listener = {
    .global = registryGlobal,
    .global_remove = registryGlobalRemove,
};

static bool createWindow(AppData& app, int width, int height, const char* title)
{
    app.width = width;
    app.height = height;

    // Connect to Wayland display
    app.display = wl_display_connect(nullptr);
    if (app.display == nullptr) {
        app.logger.logWrite("[ERROR] Failed to connect to Wayland display.");
        return false;
    }

    // Get registry and bind globals
    app.registry = wl_display_get_registry(app.display);
    wl_registry_add_listener(app.registry, &registry_listener, &app);
    wl_display_roundtrip(app.display);

    if (app.compositor == nullptr) {
        app.logger.logWrite("[ERROR] Failed to get Wayland compositor.");
        return false;
    }

    if (app.xdg_wm_base == nullptr) {
        app.logger.logWrite("[ERROR] Failed to get XDG WM base. Compositor may not support xdg-shell.");
        return false;
    }

    // Create Wayland surface
    app.surface = wl_compositor_create_surface(app.compositor);
    if (app.surface == nullptr) {
        app.logger.logWrite("[ERROR] Failed to create Wayland surface.");
        return false;
    }

    // Create XDG surface
    app.xdg_surface = xdg_wm_base_get_xdg_surface(app.xdg_wm_base, app.surface);
    if (app.xdg_surface == nullptr) {
        app.logger.logWrite("[ERROR] Failed to create XDG surface.");
        return false;
    }
    xdg_surface_add_listener(app.xdg_surface, &xdg_surface_listener, &app);

    // Create XDG toplevel (window)
    app.xdg_toplevel = xdg_surface_get_toplevel(app.xdg_surface);
    if (app.xdg_toplevel == nullptr) {
        app.logger.logWrite("[ERROR] Failed to create XDG toplevel.");
        return false;
    }
    xdg_toplevel_add_listener(app.xdg_toplevel, &xdg_toplevel_listener, &app);
    xdg_toplevel_set_title(app.xdg_toplevel, title);
    xdg_toplevel_set_app_id(app.xdg_toplevel, "com.github.MatejGomboc.StringWiggler");

    // Commit surface to trigger configure
    wl_surface_commit(app.surface);

    // Wait for configure event
    while (!app.configured) {
        wl_display_dispatch(app.display);
    }

    return true;
}

static void destroyWindow(AppData& app)
{
    if (app.xdg_toplevel != nullptr) {
        xdg_toplevel_destroy(app.xdg_toplevel);
        app.xdg_toplevel = nullptr;
    }

    if (app.xdg_surface != nullptr) {
        xdg_surface_destroy(app.xdg_surface);
        app.xdg_surface = nullptr;
    }

    if (app.surface != nullptr) {
        wl_surface_destroy(app.surface);
        app.surface = nullptr;
    }

    if (app.xdg_wm_base != nullptr) {
        xdg_wm_base_destroy(app.xdg_wm_base);
        app.xdg_wm_base = nullptr;
    }

    if (app.compositor != nullptr) {
        wl_compositor_destroy(app.compositor);
        app.compositor = nullptr;
    }

    if (app.registry != nullptr) {
        wl_registry_destroy(app.registry);
        app.registry = nullptr;
    }

    if (app.display != nullptr) {
        wl_display_disconnect(app.display);
        app.display = nullptr;
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
        destroyWindow(app);
        return EXIT_FAILURE;
    }

    Engine::NativeWindowHandle window_handle{};
    window_handle.display = app.display;
    window_handle.surface = app.surface;

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
        wl_display_dispatch_pending(app.display);
        wl_display_flush(app.display);

        // Check for errors
        if (wl_display_get_error(app.display) != 0) {
            app.logger.logWrite("[ERROR] Wayland display error.");
            break;
        }

        // Render frame here
    }

    app.renderer.destroy();
    destroyWindow(app);

    return EXIT_SUCCESS;
}
