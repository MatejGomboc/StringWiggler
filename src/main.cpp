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

#include "renderer.hpp"
#include <log/logger.hpp>
#include <window/window.hpp>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

//! Composition root — wires together the logger, the window and the Vulkan renderer.
int main()
{
    LoggingLib::Logger logger;
    logger.logInfo("StringWiggler starting.");

    WindowLib::WindowConfig config{};
    config.title = "StringWiggler";
    config.width = 800;
    config.height = 600;
    std::unique_ptr<WindowLib::Window> window{WindowLib::create(config, logger)};

    Engine::NativeWindowHandle window_handle{};
    window_handle.display = window->nativeDisplay();
    window_handle.window = window->nativeHandle();

    Engine::Renderer renderer;
    std::string error_message;
    if (!renderer.init(logger, window_handle, window->width(), window->height(), error_message)) {
        logger.logError(error_message);
        return EXIT_FAILURE;
    }

    // Track the cursor in window client pixels (start at the window centre).
    int32_t cursor_x = static_cast<int32_t>(window->width() / 2);
    int32_t cursor_y = static_cast<int32_t>(window->height() / 2);

    // Render loop — pump events non-blocking, then draw a frame. drawFrame() handles
    // swapchain recreation internally (resize / minimise) from the current window size.
    while (!window->shouldClose()) {
        window->pumpEvents();

        WindowLib::WindowEvent event;
        while (window->pollEvent(event)) {
            if (event.type == WindowLib::WindowEvent::Type::Close) {
                window->requestClose();
            } else if (event.type == WindowLib::WindowEvent::Type::MouseMove) {
                cursor_x = event.mouse_move.x;
                cursor_y = event.mouse_move.y;
            }
        }

        renderer.drawFrame(window->width(), window->height(), cursor_x, cursor_y);
    }

    renderer.destroy();
    logger.logInfo("StringWiggler shutting down.");
    return EXIT_SUCCESS;
}
