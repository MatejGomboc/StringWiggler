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

#ifdef _WIN32
#include <Volk/volk.h>
#else
#include <volk/volk.h>
#endif
#include <vulkan/vulkan_raii.hpp>
#include <algorithm>
#include <string_view>
#include <vector>

namespace Engine
{

    //! Returns true if a layer with the given name appears in the enumerated list.
    [[nodiscard]] inline bool isLayerAvailable(const std::vector<vk::LayerProperties>& available, const char* name)
    {
        return std::ranges::any_of(available, [name](const vk::LayerProperties& layer) {
            return std::string_view(layer.layerName.data()) == name;
        });
    }

    //! Returns true if an extension with the given name appears in the enumerated list.
    [[nodiscard]] inline bool isExtensionAvailable(const std::vector<vk::ExtensionProperties>& available, const char* name)
    {
        return std::ranges::any_of(available, [name](const vk::ExtensionProperties& extension) {
            return std::string_view(extension.extensionName.data()) == name;
        });
    }

} // namespace Engine
