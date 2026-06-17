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

// The string shader. Vertex positions arrive already in normalised device
// coordinates (NDC), so the vertex stage is a pass-through for now; the fragment
// stage paints a flat cyan. Both entry points compile into one SPIR-V module
// (slangc -entry vertMain -entry fragMain).

//! Per-vertex input — matches the C++ MathLib::Vec2 vertex buffer (location 0).
struct VSInput {
    [[vk::location(0)]] float2 position;
};

//! Vertex-to-fragment data.
struct VSOutput {
    float4 position : SV_Position;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fragMain() : SV_Target
{
    return float4(0.0, 0.9, 1.0, 1.0);
}
