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

#ifndef ZENRTS_DEADLINE_TIMER_H
#define ZENRTS_DEADLINE_TIMER_H

#include <zenrts/task.h>

#include <chrono>
#include <system_error>

namespace zenrts {

class Context;

class DeadlineTimer {
public:
    explicit DeadlineTimer(Context& owner);
    ~DeadlineTimer();

    DeadlineTimer(const DeadlineTimer&) = delete;
    DeadlineTimer& operator=(const DeadlineTimer&) = delete;

    void expires_after(std::chrono::nanoseconds duration);
    void expires_at(std::chrono::steady_clock::time_point time);
    void set_priority(Priority p) { prio_ = p; }

    void async_wait();
    void cancel();

protected:
    virtual void on_timer(std::error_code ec) = 0;

private:
    friend class Context;

    struct CompletionTask : TaskBase {
        DeadlineTimer* self = nullptr;
        std::error_code ec;

        void execute() override
        {
            if (self) {
                auto* t = self;
                self = nullptr;
                t->on_timer(ec);
            }
        }
    };

    Context* owner_ = nullptr;
    DeadlineTimer* prev_ = nullptr;
    DeadlineTimer* next_ = nullptr;
    std::chrono::steady_clock::time_point expiry_{};
    Priority prio_ = Priority::normal;
    bool active_ = false;
    CompletionTask completion_;
};

} // namespace zenrts

#endif
