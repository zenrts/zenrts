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

#ifndef ZENRTS_TIME_H
#define ZENRTS_TIME_H

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

namespace zenrts {

class timer {
public:
    timer() : start_(clock::now()) {}

    auto elapsed() const -> std::chrono::nanoseconds
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - start_);
    }

    auto nanos() const -> int64_t
    {
        return elapsed().count();
    }

    auto micros() const -> double
    {
        return std::chrono::duration<double, std::micro>(clock::now() - start_).count();
    }

    auto millis() const -> double
    {
        return std::chrono::duration<double, std::milli>(clock::now() - start_).count();
    }

    auto seconds() const -> double
    {
        return std::chrono::duration<double>(clock::now() - start_).count();
    }

    void reset()
    {
        start_ = clock::now();
    }

private:
    using clock = std::chrono::high_resolution_clock;
    std::chrono::time_point<clock> start_;
};

class scope_timer {
public:
    explicit scope_timer(std::string_view name, std::ostream& os = std::cout)
        : name_(name), os_(os) {}

    ~scope_timer()
    {
        auto ms = t_.millis();
        os_ << name_ << ": " << ms << " ms" << std::endl;
    }

    scope_timer(const scope_timer&) = delete;
    scope_timer& operator=(const scope_timer&) = delete;

private:
    std::string name_;
    std::ostream& os_;
    timer t_;
};

} // namespace zenrts

#endif
