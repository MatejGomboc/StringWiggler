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

#pragma once

namespace Engine
{

    //! Platform-native window identifiers, kept deliberately opaque so the renderer never
    //! includes platform headers. The two void* fields map onto the WindowLib::Window
    //! accessors: nativeDisplay() and nativeHandle().
    //!
    //! - Win32: display = HINSTANCE, window = HWND.
    //! - XCB:   display = xcb_connection_t*, window = xcb_window_t (cast through uintptr_t).
    struct NativeWindowHandle {
        void* display{nullptr}; //!< HINSTANCE (Win32) / xcb_connection_t* (XCB).
        void* window{nullptr}; //!< HWND (Win32) / xcb_window_t via uintptr_t (XCB).
    };

} // namespace Engine
