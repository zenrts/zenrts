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

#ifndef ZENRTS_STRAND_H
#define ZENRTS_STRAND_H

#include <zenrts/context.h>
#include <mutex>

namespace zenrts {

class Strand;

class StrandTaskBase : public TaskBase {
    friend class Strand;

    Strand* strand_ = nullptr;
    StrandTaskBase* next_ = nullptr;

public:
    void set_strand(Strand& s) { strand_ = &s; }

    void execute() final;

protected:
    virtual void on_execute() = 0;
};

class Strand {
public:
    explicit Strand(Context& ctx)
        : ctx_(ctx)
    {
    }

    void post(StrandTaskBase& task, Priority prio = Priority::normal)
    {
        task.prio = prio;
        task.next_ = nullptr;

        std::lock_guard lock(mtx_);
        auto was_empty = (head_ == nullptr);
        if (was_empty) {
            head_ = &task;
            tail_ = &task;
        } else {
            tail_->next_ = &task;
            tail_ = &task;
        }

        if (was_empty) {
            ctx_.post(task, prio);
        }
    }

private:
    friend class StrandTaskBase;

    void dispatch_next()
    {
        TaskBase* next = nullptr;
        {
            std::lock_guard lock(mtx_);
            if (head_) {
                // head_ is the task that just finished, advance to next
                head_ = static_cast<StrandTaskBase*>(head_->next_);
                if (!head_) tail_ = nullptr;
                next = head_;
            }
        }
        if (next) {
            ctx_.post(*next, next->prio);
        }
    }

    Context& ctx_;
    StrandTaskBase* head_ = nullptr;
    StrandTaskBase* tail_ = nullptr;
    mutable std::mutex mtx_;
};

inline void StrandTaskBase::execute()
{
    on_execute();
    if (strand_) {
        strand_->dispatch_next();
    }
}

} // namespace zenrts

#endif
