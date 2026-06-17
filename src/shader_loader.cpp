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

#include "shader_loader.hpp"
#include <fstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <linux/limits.h>
#include <unistd.h>
#endif

namespace Engine
{

    std::string executableDirectory()
    {
#ifdef _WIN32
        wchar_t wide_path[MAX_PATH]{};
        DWORD path_len{GetModuleFileNameW(nullptr, wide_path, MAX_PATH)};
        if ((path_len == 0) || (path_len >= MAX_PATH)) {
            return {};
        }
        int len{WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, nullptr, 0, nullptr, nullptr)};
        if (len <= 0) {
            return {};
        }
        std::string narrow(static_cast<std::string::size_type>(len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, narrow.data(), len, nullptr, nullptr);
        std::string::size_type pos{narrow.find_last_of("\\/")};
        return (pos != std::string::npos) ? narrow.substr(0, pos + 1) : narrow;
#else
        char path[PATH_MAX]{};
        ssize_t count{readlink("/proc/self/exe", path, PATH_MAX)};
        std::string full(path, (count > 0) ? static_cast<std::string::size_type>(count) : 0);
        std::string::size_type pos{full.find_last_of('/')};
        return (pos != std::string::npos) ? full.substr(0, pos + 1) : full;
#endif
    }

    bool loadSpirv(const std::string& path, std::vector<uint32_t>& out_code, std::string& out_error_message)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            out_error_message = "Failed to open SPIR-V file: " + path + ".";
            return false;
        }

        std::streamsize file_size{file.tellg()};
        if ((file_size <= 0) || ((file_size % static_cast<std::streamsize>(sizeof(uint32_t))) != 0)) {
            out_error_message = "Invalid SPIR-V file size: " + path + ".";
            return false;
        }

        out_code.resize(static_cast<std::vector<uint32_t>::size_type>(file_size) / sizeof(uint32_t));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(out_code.data()), file_size);
        if (!file.good()) {
            out_error_message = "Failed to read SPIR-V file: " + path + ".";
            return false;
        }
        return true;
    }

} // namespace Engine
