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

#include <volk/volk.h>
#include <string>
#include <vector>

namespace Engine
{

    //! Returns true if a layer with the given name appears in the enumerated list.
    [[nodiscard]] inline bool isLayerAvailable(const std::vector<VkLayerProperties>& available, const char* name)
    {
        for (const VkLayerProperties& layer : available) {
            if (std::string(layer.layerName) == std::string(name)) {
                return true;
            }
        }
        return false;
    }

    //! Returns true if an extension with the given name appears in the enumerated list.
    [[nodiscard]] inline bool isExtensionAvailable(const std::vector<VkExtensionProperties>& available, const char* name)
    {
        for (const VkExtensionProperties& extension : available) {
            if (std::string(extension.extensionName) == std::string(name)) {
                return true;
            }
        }
        return false;
    }

} // namespace Engine
