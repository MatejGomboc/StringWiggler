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
#include <chrono>
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

    // Render on demand: keep rendering while the string is in motion (a settle window after the
    // last input), then block on waitEvents() so an idle, settled string costs no CPU/GPU. The
    // string is damped, so it actually comes to rest within the settle window.
    constexpr float SETTLE_SECONDS = 6.0f;
    float since_input = 0.0f;
    std::chrono::steady_clock::time_point last_time = std::chrono::steady_clock::now();

    while (!window->shouldClose()) {
        // Block when settled; pump non-blocking while there is motion to show.
        if (since_input < SETTLE_SECONDS) {
            window->pumpEvents();
        } else {
            window->waitEvents();
        }

        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last_time).count();
        last_time = now;

        bool got_input = false;
        WindowLib::WindowEvent event;
        while (window->pollEvent(event)) {
            switch (event.type) {
            case WindowLib::WindowEvent::Type::Close:
                window->requestClose();
                break;
            case WindowLib::WindowEvent::Type::MouseMove:
                cursor_x = event.mouse_move.x;
                cursor_y = event.mouse_move.y;
                got_input = true;
                break;
            case WindowLib::WindowEvent::Type::Resize:
            case WindowLib::WindowEvent::Type::Expose:
                got_input = true; // need to redraw
                break;
            default:
                break;
            }
        }

        // Reset the settle timer on input; otherwise let it run down toward idle.
        since_input = got_input ? 0.0f : (since_input + dt);

        // Render only while there is something new to show (settling or just changed).
        if (since_input < SETTLE_SECONDS) {
            renderer.drawFrame(window->width(), window->height(), cursor_x, cursor_y, dt);
        }
    }

    renderer.destroy();
    logger.logInfo("StringWiggler shutting down.");
    return EXIT_SUCCESS;
}
