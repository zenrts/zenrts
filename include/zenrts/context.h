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

#ifndef ZENRTS_CONTEXT_H
#define ZENRTS_CONTEXT_H

#include <zenrts/task.h>

#include <memory>

namespace zenrts {

class DeadlineTimer;

class Context {
public:
    Context();
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    void post(TaskBase& task, Priority prio = Priority::normal);
    void run();
    void stop();
    bool stopped() const;
    void wake();

private:
    friend class DeadlineTimer;
    friend class WorkGuard;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class WorkGuard {
public:
    explicit WorkGuard(Context& ctx) noexcept;
    ~WorkGuard() noexcept;
    void reset() noexcept;

    WorkGuard(const WorkGuard&) = delete;
    WorkGuard& operator=(const WorkGuard&) = delete;

private:
    Context* ctx_;
};

} // namespace zenrts

#endif
