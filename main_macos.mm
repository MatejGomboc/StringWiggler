#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include "logger.h"
#include "renderer.h"

struct AppData {
    Engine::Logger logger;
    Engine::Renderer renderer;
};

static AppData* g_app_data = nullptr;

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

@interface MetalView : NSView
@property (nonatomic, strong) CAMetalLayer* metalLayer;
@end

@implementation MetalView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (self) {
        self.wantsLayer = YES;
        _metalLayer = [CAMetalLayer layer];
        self.layer = _metalLayer;
    }
    return self;
}

- (BOOL)wantsUpdateLayer
{
    return YES;
}

- (CALayer*)makeBackingLayer
{
    return _metalLayer;
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (nonatomic, strong) NSWindow* window;
@property (nonatomic, strong) MetalView* metalView;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    NSRect frame = NSMakeRect(0, 0, 800, 600);

    NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;

    self.window = [[NSWindow alloc] initWithContentRect:frame styleMask:style backing:NSBackingStoreBuffered defer:NO];

    [self.window setTitle:@"StringWiggler"];
    [self.window center];
    [self.window setDelegate:self];

    self.metalView = [[MetalView alloc] initWithFrame:frame];
    [self.window setContentView:self.metalView];

    [self.window makeKeyAndOrderFront:nil];

    [self initializeVulkan];
}

- (void)initializeVulkan
{
    if (g_app_data == nullptr) {
        return;
    }

    Engine::NativeWindowHandle window_handle{};
    window_handle.layer = (__bridge void*)self.metalView.metalLayer;

    std::string out_error_message;

#ifdef DEBUG
    if (!g_app_data->renderer.init(out_error_message, window_handle, vulkanDebugCallback, &g_app_data->logger)) {
#else
    if (!g_app_data->renderer.init(out_error_message, window_handle)) {
#endif
        g_app_data->logger.logWrite("[ERROR] " + out_error_message);
        [NSApp terminate:nil];
        return;
    }

    std::vector<VkPhysicalDevice> supported_devices;
    if (!g_app_data->renderer.getSupportedPhysicalDevices(supported_devices, out_error_message)) {
        g_app_data->logger.logWrite("[ERROR] " + out_error_message);
        g_app_data->renderer.destroy();
        [NSApp terminate:nil];
        return;
    }

    g_app_data->logger.logWrite("[INFO] Found supported Vulkan physical devices:");
    for (const VkPhysicalDevice& device : supported_devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        g_app_data->logger.logWrite("[INFO] \"" + std::string(props.deviceName) + "\".");
    }

    if (!g_app_data->renderer.createLogicalDevice(supported_devices[0], out_error_message)) {
        g_app_data->logger.logWrite("[ERROR] " + out_error_message);
        g_app_data->renderer.destroy();
        [NSApp terminate:nil];
        return;
    }

    VkPhysicalDeviceProperties selected_props;
    vkGetPhysicalDeviceProperties(supported_devices[0], &selected_props);
    g_app_data->logger.logWrite("[INFO] Selected \"" + std::string(selected_props.deviceName) + "\" for rendering.");
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
    return YES;
}

- (void)applicationWillTerminate:(NSNotification*)notification
{
    if (g_app_data != nullptr) {
        g_app_data->renderer.destroy();
    }
}

- (void)windowWillClose:(NSNotification*)notification
{
    [NSApp terminate:nil];
}

@end

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    AppData app_data;
    g_app_data = &app_data;

    std::string out_error_message;
    if (!app_data.logger.start("log.txt", out_error_message)) {
        return EXIT_FAILURE;
    }

    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        AppDelegate* delegate = [[AppDelegate alloc] init];
        [NSApp setDelegate:delegate];

        [NSApp activateIgnoringOtherApps:YES];
        [NSApp run];
    }

    g_app_data = nullptr;
    return EXIT_SUCCESS;
}
