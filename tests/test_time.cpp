// ZenRTS
// Copyright (C) 2026  Ian Torres
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License version 3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <gtest/gtest.h>
#include <zenrts/time.h>
#include <thread>

TEST(TimeTest, TimerMeasuresElapsed)
{
    zenrts::timer t;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto ms = t.millis();
    EXPECT_GE(ms, 5.0);
}

TEST(TimeTest, TimerNanos)
{
    zenrts::timer t;
    auto ns = t.nanos();
    EXPECT_GE(ns, 0);
}

TEST(TimeTest, TimerReset)
{
    zenrts::timer t;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto before = t.millis();
    t.reset();
    auto after = t.millis();
    EXPECT_GE(before, after);
}

TEST(TimeTest, TimerSeconds)
{
    zenrts::timer t;
    auto s = t.seconds();
    EXPECT_GE(s, 0.0);
}

TEST(TimeTest, ScopeTimer)
{
    testing::internal::CaptureStdout();
    {
        zenrts::scope_timer st("test");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    auto output = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(output.find("test:") != std::string::npos);
    EXPECT_TRUE(output.find("ms") != std::string::npos);
}
