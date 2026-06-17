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

#include "testing/testing.hpp"
#include <math/vector.hpp>
#include <cmath>

using MathLib::Vec2;
using MathLib::Vec3;

// Expected values use named locals rather than inline braced initialisers: a brace-comma
// such as Vec2{4.0f, 6.0f} inside TEST_CHECK(...) would be split by the preprocessor into
// two macro arguments (only parentheses, not braces, protect commas).

//! Approximate float equality for length/normalise checks.
[[nodiscard]] static bool approx(float a, float b)
{
    return std::fabs(a - b) < 1e-5f;
}

TEST_CASE(vec2_arithmetic)
{
    Vec2 a{1.0f, 2.0f};
    Vec2 b{3.0f, 4.0f};
    Vec2 expected_sum{4.0f, 6.0f};
    Vec2 expected_diff{2.0f, 2.0f};
    Vec2 expected_scaled{2.0f, 4.0f};
    Vec2 expected_negated{-1.0f, -2.0f};
    TEST_CHECK(a + b == expected_sum);
    TEST_CHECK(b - a == expected_diff);
    TEST_CHECK(a * 2.0f == expected_scaled);
    TEST_CHECK(-a == expected_negated);
}

TEST_CASE(vec2_compound_assignment)
{
    Vec2 a{1.0f, 1.0f};
    Vec2 add{2.0f, 3.0f};
    a += add;
    Vec2 expected_after_add{3.0f, 4.0f};
    TEST_CHECK(a == expected_after_add);

    Vec2 sub{1.0f, 1.0f};
    a -= sub;
    Vec2 expected_after_sub{2.0f, 3.0f};
    TEST_CHECK(a == expected_after_sub);

    a *= 2.0f;
    Vec2 expected_after_scale{4.0f, 6.0f};
    TEST_CHECK(a == expected_after_scale);
}

TEST_CASE(vec2_dot_and_length)
{
    Vec2 a{3.0f, 4.0f};
    TEST_CHECK(approx(a.dot(a), 25.0f));
    TEST_CHECK(approx(a.lengthSquared(), 25.0f));
    TEST_CHECK(approx(a.length(), 5.0f));
}

TEST_CASE(vec2_normalise)
{
    Vec2 a{3.0f, 4.0f};
    Vec2 n{a.normalised()};
    TEST_CHECK(approx(n.length(), 1.0f));

    // Zero-length vector normalises to zero (guards against NaN).
    Vec2 zero{0.0f, 0.0f};
    Vec2 expected_zero{0.0f, 0.0f};
    TEST_CHECK(zero.normalised() == expected_zero);
}

TEST_CASE(vec3_cross)
{
    Vec3 x{1.0f, 0.0f, 0.0f};
    Vec3 y{0.0f, 1.0f, 0.0f};
    Vec3 expected_z{0.0f, 0.0f, 1.0f};
    TEST_CHECK(x.cross(y) == expected_z);
}

int main()
{
    return static_cast<int>(TestingLib::runAll());
}
