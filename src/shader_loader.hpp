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

#include <cstdint>
#include <string>
#include <vector>

namespace Engine
{

    //! Returns the directory containing the running executable (with a trailing separator).
    [[nodiscard]] std::string executableDirectory();

    //! Reads a SPIR-V binary into a uint32 buffer. Returns false and fills out_error_message
    //! on failure.
    [[nodiscard]] bool loadSpirv(const std::string& path, std::vector<uint32_t>& out_code, std::string& out_error_message);

} // namespace Engine
