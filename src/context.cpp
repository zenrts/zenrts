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

#include <zenrts/context.h>
#include <zenrts/deadline_timer.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace zenrts {

// ── Context::Impl ──

struct Context::Impl {
    static constexpr int kNumPriorities = 4;

    std::atomic<TaskBase*> ready_heads_[kNumPriorities]{};
    DeadlineTimer* timer_head_ = nullptr;

    int work_count_ = 0;
    std::atomic<bool> stopped_{false};
    mutable std::mutex mtx_;
    std::condition_variable cv_;

    // Internal operations
    void lock_free_push(TaskBase& task, Priority prio);
    TaskBase* drain_all();
    bool has_work() const;
    void insert_timer(DeadlineTimer& timer);
    void remove_timer(DeadlineTimer& timer);
    void fire_expired_timers();
};

void Context::Impl::lock_free_push(TaskBase& task, Priority prio)
{
    auto& head = ready_heads_[static_cast<int>(prio)];
    auto expected = head.load(std::memory_order_relaxed);
    do {
        task.next.store(expected, std::memory_order_relaxed);
    } while (!head.compare_exchange_weak(expected, &task,
        std::memory_order_release, std::memory_order_relaxed));
}

TaskBase* Context::Impl::drain_all()
{
    TaskBase* stacks[kNumPriorities];
    for (int i = 0; i < kNumPriorities; ++i) {
        stacks[i] = ready_heads_[i].exchange(nullptr, std::memory_order_acquire);
    }

    TaskBase* result = nullptr;
    TaskBase* tail = nullptr;

    for (int i = kNumPriorities - 1; i >= 0; --i) {
        auto* cur = stacks[i];
        if (!cur) continue;

        TaskBase* prev = nullptr;
        while (cur) {
            auto* next = cur->next.load(std::memory_order_relaxed);
            cur->next.store(prev, std::memory_order_relaxed);
            prev = cur;
            cur = next;
        }

        if (!result) {
            result = prev;
        } else {
            tail->next.store(prev, std::memory_order_relaxed);
        }

        while (prev->next.load(std::memory_order_relaxed)) {
            prev = prev->next.load(std::memory_order_relaxed);
        }
        tail = prev;
    }

    return result;
}

bool Context::Impl::has_work() const
{
    for (int i = 0; i < kNumPriorities; ++i) {
        if (ready_heads_[i].load(std::memory_order_relaxed) != nullptr) {
            return true;
        }
    }
    return false;
}

void Context::Impl::insert_timer(DeadlineTimer& timer)
{
    timer.prev_ = nullptr;
    timer.next_ = nullptr;

    if (!timer_head_) {
        timer_head_ = &timer;
        return;
    }

    if (timer.expiry_ < timer_head_->expiry_) {
        timer.next_ = timer_head_;
        timer_head_->prev_ = &timer;
        timer_head_ = &timer;
        return;
    }

    auto* cur = timer_head_;
    while (cur->next_ && cur->next_->expiry_ <= timer.expiry_) {
        cur = cur->next_;
    }

    timer.next_ = cur->next_;
    timer.prev_ = cur;
    if (cur->next_) {
        cur->next_->prev_ = &timer;
    }
    cur->next_ = &timer;
}

void Context::Impl::remove_timer(DeadlineTimer& timer)
{
    if (timer.prev_) {
        timer.prev_->next_ = timer.next_;
    } else {
        timer_head_ = timer.next_;
    }
    if (timer.next_) {
        timer.next_->prev_ = timer.prev_;
    }
    timer.prev_ = nullptr;
    timer.next_ = nullptr;
}

void Context::Impl::fire_expired_timers()
{
    auto now = std::chrono::steady_clock::now();
    while (timer_head_ && timer_head_->expiry_ <= now) {
        auto& timer = *timer_head_;
        remove_timer(timer);
        timer.active_ = false;

        timer.completion_.self = &timer;
        timer.completion_.ec = std::error_code{};
        lock_free_push(timer.completion_, timer.prio_);
    }
}

// ── Context ──

Context::Context()
    : impl_(std::make_unique<Impl>())
{
}

Context::~Context()
{
    stop();
}

void Context::post(TaskBase& task, Priority prio)
{
    impl_->lock_free_push(task, prio);

    {
        std::lock_guard lock(impl_->mtx_);
        impl_->cv_.notify_one();
    }
}

void Context::run()
{
    for (;;) {
        if (impl_->stopped_.load(std::memory_order_acquire)) break;

        {
            std::unique_lock lock(impl_->mtx_);
            impl_->fire_expired_timers();
        }

        if (impl_->stopped_.load(std::memory_order_acquire)) break;

        auto* tasks = impl_->drain_all();
        while (tasks && !impl_->stopped_.load(std::memory_order_acquire)) {
            auto* task = tasks;
            tasks = task->next.load(std::memory_order_relaxed);
            task->next.store(nullptr, std::memory_order_relaxed);
            task->execute();
        }

        if (impl_->stopped_.load(std::memory_order_acquire)) break;

        {
            std::unique_lock lock(impl_->mtx_);
            if (impl_->stopped_.load(std::memory_order_acquire)) break;

            if (impl_->has_work()) continue;

            if (impl_->work_count_ > 0 || impl_->timer_head_) {
                auto next = impl_->timer_head_
                    ? impl_->timer_head_->expiry_
                    : std::chrono::steady_clock::now() + std::chrono::seconds(1);
                impl_->cv_.wait_until(lock, next);
            } else {
                break;
            }
        }
    }
}

void Context::stop()
{
    impl_->stopped_.store(true, std::memory_order_release);
    {
        std::lock_guard lock(impl_->mtx_);
        impl_->cv_.notify_all();
    }
}

bool Context::stopped() const
{
    return impl_->stopped_.load(std::memory_order_acquire);
}

void Context::wake()
{
    impl_->cv_.notify_one();
}

// ── WorkGuard ──

WorkGuard::WorkGuard(Context& ctx) noexcept
    : ctx_(&ctx)
{
    std::lock_guard lock(ctx_->impl_->mtx_);
    ++ctx_->impl_->work_count_;
}

WorkGuard::~WorkGuard() noexcept
{
    if (ctx_) {
        reset();
    }
}

void WorkGuard::reset() noexcept
{
    if (ctx_) {
        {
            std::lock_guard lock(ctx_->impl_->mtx_);
            --ctx_->impl_->work_count_;
        }
        ctx_->impl_->cv_.notify_one();
        ctx_ = nullptr;
    }
}

// ── DeadlineTimer ──

DeadlineTimer::DeadlineTimer(Context& owner)
    : owner_(&owner)
{
}

DeadlineTimer::~DeadlineTimer()
{
    if (owner_) {
        {
            std::lock_guard lock(owner_->impl_->mtx_);
            if (active_) {
                owner_->impl_->remove_timer(*this);
                active_ = false;
            }
        }
        completion_.self = nullptr;
    }
}

void DeadlineTimer::expires_after(std::chrono::nanoseconds duration)
{
    expires_at(std::chrono::steady_clock::now() + duration);
}

void DeadlineTimer::expires_at(std::chrono::steady_clock::time_point time)
{
    std::lock_guard lock(owner_->impl_->mtx_);
    expiry_ = time;
    if (active_) {
        owner_->impl_->remove_timer(*this);
        active_ = false;
    }
}

void DeadlineTimer::async_wait()
{
    {
        std::lock_guard lock(owner_->impl_->mtx_);
        if (active_) {
            owner_->impl_->remove_timer(*this);
        }
        active_ = true;
        owner_->impl_->insert_timer(*this);
    }
    owner_->wake();
}

void DeadlineTimer::cancel()
{
    bool should_post = false;
    {
        std::lock_guard lock(owner_->impl_->mtx_);
        if (active_) {
            owner_->impl_->remove_timer(*this);
            active_ = false;
            should_post = true;
        }
    }

    if (should_post) {
        completion_.self = this;
        completion_.ec = std::make_error_code(std::errc::operation_canceled);
        owner_->post(completion_, prio_);
    }
}

} // namespace zenrts
