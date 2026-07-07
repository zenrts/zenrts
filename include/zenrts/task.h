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

#ifndef ZENRTS_TASK_H
#define ZENRTS_TASK_H

#include <atomic>
#include <cstdint>
#include <utility>

namespace zenrts {

enum class Priority : uint8_t {
    low = 0,
    normal = 1,
    high = 2,
    critical = 3,
};

struct TaskBase {
    std::atomic<TaskBase*> next{nullptr};
    Priority prio = Priority::normal;

    virtual void execute() = 0;

protected:
    ~TaskBase() = default;

    TaskBase() = default;
    TaskBase(const TaskBase&) = delete;
    TaskBase& operator=(const TaskBase&) = delete;
    TaskBase(TaskBase&&) = delete;
    TaskBase& operator=(TaskBase&&) = delete;
};

template<typename F>
class Task final : public TaskBase {
    F fn_;

public:
    explicit Task(F&& fn)
        : fn_(std::move(fn))
    {
    }

    void execute() override
    {
        fn_();
    }
};

} // namespace zenrts

#endif
