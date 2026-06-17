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
#include <stdexcept>

TEST_CASE(check_passes)
{
    TEST_CHECK(true);
    TEST_CHECK(1 + 1 == 2);
}

TEST_CASE(check_equal_passes)
{
    TEST_CHECK_EQUAL(2 + 2, 4);
    TEST_CHECK_EQUAL(std::string("hello"), std::string("hello"));
}

TEST_CASE(check_throws_detects_throw)
{
    TEST_CHECK_THROWS(throw std::runtime_error("boom"));
}

int main()
{
    return static_cast<int>(TestingLib::runAll());
}
