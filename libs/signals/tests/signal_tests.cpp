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
#include <signal/signal.hpp>
#include <thread>

TEST_CASE(signal_emit_and_consume)
{
    SignalsLib::Signal<int> signal;
    TEST_CHECK(signal.empty());

    signal.emit(42);
    TEST_CHECK(!signal.empty());
    TEST_CHECK_EQUAL(signal.size(), static_cast<std::size_t>(1));

    int out{0};
    TEST_CHECK(signal.consume(out));
    TEST_CHECK_EQUAL(out, 42);
    TEST_CHECK(signal.empty());
}

TEST_CASE(signal_consume_empty_returns_false)
{
    SignalsLib::Signal<int> signal;
    int out{-1};
    TEST_CHECK(!signal.consume(out));
}

TEST_CASE(signal_fifo_order)
{
    SignalsLib::Signal<int> signal;
    signal.emit(1);
    signal.emit(2);
    signal.emit(3);

    int out{0};
    TEST_CHECK(signal.consume(out));
    TEST_CHECK_EQUAL(out, 1);
    TEST_CHECK(signal.consume(out));
    TEST_CHECK_EQUAL(out, 2);
    TEST_CHECK(signal.consume(out));
    TEST_CHECK_EQUAL(out, 3);
}

TEST_CASE(signal_thread_safety)
{
    SignalsLib::Signal<int> signal;
    constexpr int count{1000};

    std::thread producer([&] {
        for (int i{0}; i < count; ++i) {
            signal.emit(i);
        }
    });

    int consumed{0};
    std::thread consumer([&] {
        int out{0};
        while (consumed < count) {
            if (signal.consume(out)) {
                ++consumed;
            }
        }
    });

    producer.join();
    consumer.join();
    TEST_CHECK_EQUAL(consumed, count);
    TEST_CHECK(signal.empty());
}

int main()
{
    return static_cast<int>(TestingLib::runAll());
}
