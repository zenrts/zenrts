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
#include <zenrts/context.h>
#include <zenrts/deadline_timer.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace {

struct TestTimer : zenrts::DeadlineTimer {
    using DeadlineTimer::DeadlineTimer;

    std::error_code result;
    bool fired = false;
    std::mutex mtx;
    std::condition_variable cv;

    void on_timer(std::error_code ec) override
    {
        {
            std::lock_guard lock(mtx);
            result = ec;
            fired = true;
        }
        cv.notify_one();
    }

    bool wait_for_fire(std::chrono::milliseconds timeout = std::chrono::seconds(2))
    {
        std::unique_lock lock(mtx);
        return cv.wait_for(lock, timeout, [this] { return fired; });
    }

    void reset_state()
    {
        std::lock_guard lock(mtx);
        result = {};
        fired = false;
    }
};

struct OrderTask : zenrts::TaskBase {
    int id;
    std::vector<int>& order;
    std::mutex& mtx;

    OrderTask(int i, std::vector<int>& o, std::mutex& m)
        : id(i), order(o), mtx(m)
    {
    }

    void execute() override
    {
        std::lock_guard lock(mtx);
        order.push_back(id);
    }
};

struct ThreadGuard {
    std::thread& t;

    explicit ThreadGuard(std::thread& th)
        : t(th)
    {
    }

    ~ThreadGuard()
    {
        if (t.joinable()) {
            t.join();
        }
    }
};

} // namespace

TEST(DeadlineTimerTest, TimerFires)
{
    zenrts::Context ctx;
    TestTimer timer(ctx);
    timer.expires_after(std::chrono::milliseconds(10));
    timer.async_wait();

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    EXPECT_TRUE(timer.wait_for_fire());
    EXPECT_FALSE(timer.result);

    ctx.stop();
}

TEST(DeadlineTimerTest, TimerCancel)
{
    zenrts::Context ctx;
    TestTimer timer(ctx);
    timer.expires_after(std::chrono::hours(1));
    timer.async_wait();

    zenrts::WorkGuard guard(ctx);

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    timer.cancel();

    EXPECT_TRUE(timer.wait_for_fire());
    EXPECT_EQ(timer.result, std::make_error_code(std::errc::operation_canceled));

    ctx.stop();
}

TEST(DeadlineTimerTest, TimerReuse)
{
    zenrts::Context ctx;
    TestTimer timer(ctx);

    zenrts::WorkGuard guard(ctx);

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    timer.expires_after(std::chrono::milliseconds(10));
    timer.async_wait();
    EXPECT_TRUE(timer.wait_for_fire());
    EXPECT_FALSE(timer.result);
    timer.reset_state();

    timer.expires_after(std::chrono::milliseconds(10));
    timer.async_wait();
    EXPECT_TRUE(timer.wait_for_fire());
    EXPECT_FALSE(timer.result);

    ctx.stop();
}

TEST(DeadlineTimerTest, TimerExpiresAt)
{
    zenrts::Context ctx;
    TestTimer timer(ctx);
    auto tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(10);
    timer.expires_at(tp);
    timer.async_wait();

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    EXPECT_TRUE(timer.wait_for_fire());
    EXPECT_FALSE(timer.result);

    ctx.stop();
}

TEST(DeadlineTimerTest, MultipleTimersFireInOrder)
{
    zenrts::Context ctx;
    std::vector<int> order;
    std::mutex mtx;

    struct TimerRec : zenrts::DeadlineTimer {
        int id;
        std::vector<int>& order;
        std::mutex& mtx;

        TimerRec(zenrts::Context& ctx, int i, std::vector<int>& o, std::mutex& m)
            : DeadlineTimer(ctx), id(i), order(o), mtx(m)
        {
        }

        void on_timer(std::error_code) override
        {
            std::lock_guard lock(mtx);
            order.push_back(id);
        }
    };

    TimerRec fast(ctx, 1, order, mtx);
    TimerRec medium(ctx, 2, order, mtx);
    TimerRec slow(ctx, 3, order, mtx);

    fast.expires_after(std::chrono::milliseconds(10));
    medium.expires_after(std::chrono::milliseconds(30));
    slow.expires_after(std::chrono::milliseconds(50));

    fast.async_wait();
    medium.async_wait();
    slow.async_wait();

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ASSERT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);

    ctx.stop();
}

// ── Edge case: timer inserted at head of timer queue ──

TEST(DeadlineTimerTest, TimerInsertAtHead)
{
    zenrts::Context ctx;

    struct Rec : zenrts::DeadlineTimer {
        int id;
        std::vector<int>& order;
        std::mutex& mtx;

        Rec(zenrts::Context& ctx, int i, std::vector<int>& o, std::mutex& m)
            : DeadlineTimer(ctx), id(i), order(o), mtx(m)
        {
        }

        void on_timer(std::error_code) override
        {
            std::lock_guard lock(mtx);
            order.push_back(id);
        }
    };

    std::vector<int> order;
    std::mutex mtx;
    Rec later(ctx, 1, order, mtx);
    Rec earlier(ctx, 2, order, mtx);

    later.expires_after(std::chrono::milliseconds(30));
    later.async_wait();

    earlier.expires_after(std::chrono::milliseconds(10));
    earlier.async_wait();

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_EQ(order.size(), 2);
    EXPECT_EQ(order[0], 2);
    EXPECT_EQ(order[1], 1);

    ctx.stop();
}

// ── Edge case: cancel timer in the middle of the queue ──

TEST(DeadlineTimerTest, CancelMiddleTimer)
{
    zenrts::Context ctx;

    struct Rec : zenrts::DeadlineTimer {
        int id;
        std::vector<int>& order;
        std::mutex& mtx;

        Rec(zenrts::Context& ctx, int i, std::vector<int>& o, std::mutex& m)
            : DeadlineTimer(ctx), id(i), order(o), mtx(m)
        {
        }

        void on_timer(std::error_code ec) override
        {
            std::lock_guard lock(mtx);
            if (!ec) order.push_back(id);
        }
    };

    std::vector<int> order;
    std::mutex mtx;

    zenrts::WorkGuard guard(ctx);

    Rec first(ctx, 1, order, mtx);
    Rec middle(ctx, 2, order, mtx);
    Rec last(ctx, 3, order, mtx);

    first.expires_after(std::chrono::milliseconds(10));
    middle.expires_after(std::chrono::milliseconds(30));
    last.expires_after(std::chrono::milliseconds(50));

    first.async_wait();
    middle.async_wait();
    last.async_wait();

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    middle.cancel();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_EQ(order.size(), 2);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 3);

    ctx.stop();
}

// ── Edge case: expires_at while timer is active ──

TEST(DeadlineTimerTest, ExpiresAtWhileActive)
{
    zenrts::Context ctx;

    struct Rec : zenrts::DeadlineTimer {
        using DeadlineTimer::DeadlineTimer;
        bool fired = false;
        std::mutex mtx;
        std::condition_variable cv;

        void on_timer(std::error_code) override
        {
            std::lock_guard lock(mtx);
            fired = true;
            cv.notify_one();
        }

        bool wait_for_fire(std::chrono::milliseconds timeout = std::chrono::seconds(2))
        {
            std::unique_lock lock(mtx);
            return cv.wait_for(lock, timeout, [this] { return fired; });
        }
    };

    Rec timer(ctx);
    timer.expires_after(std::chrono::hours(1));
    timer.async_wait();

    timer.expires_after(std::chrono::milliseconds(10));
    timer.async_wait();

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    EXPECT_TRUE(timer.wait_for_fire());

    ctx.stop();
}

// ── Edge case: async_wait while already active ──

TEST(DeadlineTimerTest, AsyncWaitWhileActive)
{
    zenrts::Context ctx;

    struct Rec : zenrts::DeadlineTimer {
        using DeadlineTimer::DeadlineTimer;
        bool fired = false;
        std::mutex mtx;
        std::condition_variable cv;

        void on_timer(std::error_code) override
        {
            std::lock_guard lock(mtx);
            fired = true;
            cv.notify_one();
        }

        bool wait_for_fire(std::chrono::milliseconds timeout = std::chrono::seconds(2))
        {
            std::unique_lock lock(mtx);
            return cv.wait_for(lock, timeout, [this] { return fired; });
        }
    };

    Rec timer(ctx);
    timer.expires_after(std::chrono::hours(1));
    timer.async_wait();

    timer.expires_after(std::chrono::milliseconds(10));
    timer.async_wait();

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    EXPECT_TRUE(timer.wait_for_fire());

    ctx.stop();
}

// ── Edge case: destroy timer while active (no crash) ──

TEST(DeadlineTimerTest, DestroyWhileActive)
{
    zenrts::Context ctx;
    zenrts::WorkGuard guard(ctx);

    {
        struct Rec : zenrts::DeadlineTimer {
            using DeadlineTimer::DeadlineTimer;
            void on_timer(std::error_code) override {}
        };

        Rec timer(ctx);
        timer.expires_after(std::chrono::hours(1));
        timer.async_wait();
    }

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ctx.stop();
}

// ── Edge case: insert timer in the middle of the queue ──

TEST(DeadlineTimerTest, TimerInsertMiddle)
{
    zenrts::Context ctx;

    struct Rec : zenrts::DeadlineTimer {
        int id;
        std::vector<int>& order;
        std::mutex& mtx;

        Rec(zenrts::Context& ctx, int i, std::vector<int>& o, std::mutex& m)
            : DeadlineTimer(ctx), id(i), order(o), mtx(m)
        {
        }

        void on_timer(std::error_code) override
        {
            std::lock_guard lock(mtx);
            order.push_back(id);
        }
    };

    std::vector<int> order;
    std::mutex mtx;
    Rec first(ctx, 1, order, mtx);
    Rec last(ctx, 3, order, mtx);
    Rec middle(ctx, 2, order, mtx);

    first.expires_after(std::chrono::milliseconds(10));
    first.async_wait();

    last.expires_after(std::chrono::milliseconds(30));
    last.async_wait();

    middle.expires_after(std::chrono::milliseconds(20));
    middle.async_wait();

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_EQ(order.size(), 3);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);

    ctx.stop();
}

// ── Edge case: async_wait while another wait is active (same expiry) ──

TEST(DeadlineTimerTest, AsyncWaitDoubleCall)
{
    zenrts::Context ctx;

    struct Rec : zenrts::DeadlineTimer {
        using DeadlineTimer::DeadlineTimer;
        bool fired = false;
        std::mutex mtx;
        std::condition_variable cv;

        void on_timer(std::error_code) override
        {
            std::lock_guard lock(mtx);
            fired = true;
            cv.notify_one();
        }

        bool wait_for_fire(std::chrono::milliseconds timeout = std::chrono::seconds(2))
        {
            std::unique_lock lock(mtx);
            return cv.wait_for(lock, timeout, [this] { return fired; });
        }
    };

    Rec timer(ctx);
    timer.expires_after(std::chrono::milliseconds(10));
    timer.async_wait();
    timer.async_wait();

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    EXPECT_TRUE(timer.wait_for_fire());

    ctx.stop();
}

