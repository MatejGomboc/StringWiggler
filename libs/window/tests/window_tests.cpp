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

// Tests exercise only the platform-neutral WindowEvent POD — no live window or display
// connection is created, so these run headless in CI.

#include "testing/testing.hpp"
#include <window/window_event.hpp>

using WindowLib::WindowEvent;

TEST_CASE(event_default_is_none)
{
    WindowEvent ev;
    TEST_CHECK(ev.type == WindowEvent::Type::None);
}

TEST_CASE(event_explicit_type)
{
    WindowEvent ev{WindowEvent::Type::Close};
    TEST_CHECK(ev.type == WindowEvent::Type::Close);
}

TEST_CASE(event_resize_data)
{
    WindowEvent ev{WindowEvent::Type::Resize};
    ev.resize.width = 1280;
    ev.resize.height = 720;
    TEST_CHECK_EQUAL(ev.resize.width, static_cast<uint32_t>(1280));
    TEST_CHECK_EQUAL(ev.resize.height, static_cast<uint32_t>(720));
}

TEST_CASE(event_mouse_move_data)
{
    WindowEvent ev{WindowEvent::Type::MouseMove};
    ev.mouse_move.x = 10;
    ev.mouse_move.y = 20;
    ev.mouse_move.dx = -3;
    ev.mouse_move.dy = 4;
    TEST_CHECK_EQUAL(ev.mouse_move.x, 10);
    TEST_CHECK_EQUAL(ev.mouse_move.y, 20);
    TEST_CHECK_EQUAL(ev.mouse_move.dx, -3);
    TEST_CHECK_EQUAL(ev.mouse_move.dy, 4);
}

int main()
{
    return static_cast<int>(TestingLib::runAll());
}
