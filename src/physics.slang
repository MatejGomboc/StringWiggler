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

// GPU string physics — one thread per node, the whole string in a single workgroup.
// Verlet integration with gravity, then distance constraints solved in shared memory with
// even/odd (red-black) Gauss-Seidel passes synchronised by GroupMemoryBarrierWithGroupSync.
// The head (node 0) is pinned to the cursor. All coordinates are normalised device
// coordinates (NDC); Vulkan's clip space is +Y down, so gravity is positive Y.

// Must match Renderer::NODE_COUNT (C++).
static const uint NODE_COUNT = 128;

struct PhysicsPush {
    float2 cursor; //!< Head target (NDC).
    float dt; //!< Frame delta time (seconds, clamped).
    float gravity; //!< Downward acceleration (NDC / s^2; +Y is down).
    float segment_length; //!< Rest distance between adjacent nodes (NDC).
    float damping; //!< Velocity damping per step (0..1) so the string loses energy and settles.
    uint node_count; //!< Active node count.
    uint iterations; //!< Constraint relaxation iterations.
};

[[vk::push_constant]]
PhysicsPush pc;

//! Current node positions (read-write; also bound as the vertex buffer).
[[vk::binding(0, 0)]]
RWStructuredBuffer<float2> positions;

//! Previous node positions (read-write; Verlet history).
[[vk::binding(1, 0)]]
RWStructuredBuffer<float2> prev_positions;

//! Shared working set for the whole string.
groupshared float2 g_pos[NODE_COUNT];

//! Solves the distance constraint between nodes a and b in shared memory.
void solveConstraint(uint a, uint b)
{
    float2 pa = g_pos[a];
    float2 pb = g_pos[b];
    float2 delta = pb - pa;
    float dist = length(delta);
    if (dist > 1e-6) {
        float diff = (dist - pc.segment_length) / dist;
        float2 correction = delta * (0.5 * diff);
        g_pos[a] = pa + correction;
        g_pos[b] = pb - correction;
    }
}

[shader("compute")]
[numthreads(NODE_COUNT, 1, 1)]
void physicsMain(uint3 thread_id: SV_DispatchThreadID)
{
    uint i = thread_id.x;

    // Verlet integration (per node; no neighbour access, so in-place is safe).
    float2 pos = positions[i];
    float2 prev = prev_positions[i];
    float2 velocity = pos - prev;
    float2 accel = float2(0.0, pc.gravity);
    float2 next = pos + velocity * pc.damping + accel * (pc.dt * pc.dt);

    prev_positions[i] = pos; // becomes the previous position for next frame
    g_pos[i] = next;
    GroupMemoryBarrierWithGroupSync();

    // Pin the head to the cursor.
    if (i == 0) {
        g_pos[0] = pc.cursor;
    }
    GroupMemoryBarrierWithGroupSync();

    // Distance constraints — red-black Gauss-Seidel. Even constraints (i,i+1) for even i are
    // mutually disjoint and safe in parallel; likewise odd. Re-pin the head each iteration so
    // its constraint effectively only moves node 1.
    for (uint it = 0; it < pc.iterations; ++it) {
        if (((i & 1u) == 0u) && (i + 1u < pc.node_count)) {
            solveConstraint(i, i + 1u);
        }
        GroupMemoryBarrierWithGroupSync();

        if (((i & 1u) == 1u) && (i + 1u < pc.node_count)) {
            solveConstraint(i, i + 1u);
        }
        GroupMemoryBarrierWithGroupSync();

        if (i == 0) {
            g_pos[0] = pc.cursor;
        }
        GroupMemoryBarrierWithGroupSync();
    }

    positions[i] = g_pos[i];
}
