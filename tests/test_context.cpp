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
#include <atomic>
#include <thread>
#include <vector>

namespace {

struct CounterTask : zenrts::TaskBase {
    std::atomic<int>& counter;

    explicit CounterTask(std::atomic<int>& c)
        : counter(c)
    {
    }

    void execute() override
    {
        counter.fetch_add(1, std::memory_order_relaxed);
    }
};

struct CollectTask : zenrts::TaskBase {
    int id;
    std::vector<int>& order;
    std::mutex& mtx;

    CollectTask(int i, std::vector<int>& o, std::mutex& m)
        : id(i), order(o), mtx(m)
    {
    }

    void execute() override
    {
        std::lock_guard lock(mtx);
        order.push_back(id);
    }
};

struct StopTask : zenrts::TaskBase {
    zenrts::Context& ctx;

    explicit StopTask(zenrts::Context& c)
        : ctx(c)
    {
    }

    void execute() override
    {
        ctx.stop();
    }
};

} // namespace

TEST(ContextTest, PostRunsTask)
{
    zenrts::Context ctx;
    std::atomic<int> counter{0};
    CounterTask task(counter);

    ctx.post(task);

    std::thread worker([&ctx] { ctx.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ctx.stop();
    worker.join();

    EXPECT_EQ(counter.load(), 1);
}

TEST(ContextTest, PostMultipleTasks)
{
    zenrts::Context ctx;
    std::vector<int> order;
    std::mutex mtx;
    CollectTask t1(1, order, mtx);
    CollectTask t2(2, order, mtx);
    CollectTask t3(3, order, mtx);

    ctx.post(t1);
    ctx.post(t2);
    ctx.post(t3);

    std::thread worker([&ctx] { ctx.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ctx.stop();
    worker.join();

    EXPECT_EQ(order.size(), 3);
}

TEST(ContextTest, PriorityExecution)
{
    zenrts::Context ctx;
    std::vector<int> order;
    std::mutex mtx;
    CollectTask low(1, order, mtx);
    CollectTask normal(2, order, mtx);
    CollectTask high(3, order, mtx);
    CollectTask critical(4, order, mtx);

    ctx.post(low, zenrts::Priority::low);
    ctx.post(normal, zenrts::Priority::normal);
    ctx.post(high, zenrts::Priority::high);
    ctx.post(critical, zenrts::Priority::critical);

    std::thread worker([&ctx] { ctx.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ctx.stop();
    worker.join();

    ASSERT_EQ(order.size(), 4);
    EXPECT_EQ(order[0], 4);
    EXPECT_EQ(order[1], 3);
    EXPECT_EQ(order[2], 2);
    EXPECT_EQ(order[3], 1);
}

TEST(ContextTest, StopPreventsExecution)
{
    zenrts::Context ctx;
    std::atomic<int> counter{0};
    CounterTask task(counter);

    ctx.stop();
    ctx.post(task);

    std::thread worker([&ctx] { ctx.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    worker.join();

    EXPECT_EQ(counter.load(), 0);
}

TEST(ContextTest, StopInsideTask)
{
    zenrts::Context ctx;
    std::atomic<int> counter{0};
    CounterTask c1(counter);
    CounterTask c2(counter);
    StopTask stop(ctx);

    ctx.post(c1);
    ctx.post(stop);
    ctx.post(c2);

    ctx.run();

    EXPECT_EQ(counter.load(), 1);
}

TEST(ContextTest, RunReturnsWithoutWork)
{
    zenrts::Context ctx;
    ctx.run();
}

TEST(ContextTest, WorkGuardPreventsReturn)
{
    zenrts::Context ctx;
    std::atomic<bool> started{false};

    zenrts::WorkGuard guard(ctx);

    std::thread worker([&ctx, &started] {
        started.store(true);
        ctx.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(started.load());
    EXPECT_FALSE(ctx.stopped());

    guard.reset();
    worker.join();
}

TEST(ContextTest, WorkGuardReset)
{
    zenrts::Context ctx;
    std::atomic<int> counter{0};

    {
        zenrts::WorkGuard guard(ctx);
        std::thread worker([&ctx] { ctx.run(); });

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        CounterTask task(counter);
        ctx.post(task);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        guard.reset();
        worker.join();
    }

    EXPECT_EQ(counter.load(), 1);
}

TEST(ContextTest, StoppedState)
{
    zenrts::Context ctx;
    EXPECT_FALSE(ctx.stopped());
    ctx.stop();
    EXPECT_TRUE(ctx.stopped());
}

TEST(ContextTest, PostFromAnyThread)
{
    zenrts::Context ctx;
    std::atomic<int> counter{0};
    CounterTask task(counter);

    zenrts::WorkGuard guard(ctx);

    std::thread worker([&ctx] { ctx.run(); });

    std::thread poster([&ctx, &task] {
        ctx.post(task);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    poster.join();

    EXPECT_EQ(counter.load(), 1);

    ctx.stop();
    worker.join();
}

TEST(ContextTest, TaskPostsFromExecution)
{
    zenrts::Context ctx;
    std::atomic<int> counter{0};

    struct SelfPoster : zenrts::TaskBase {
        zenrts::Context& ctx;
        std::atomic<int>& counter;
        SelfPoster* child;

        SelfPoster(zenrts::Context& c, std::atomic<int>& cnt, SelfPoster* ch)
            : ctx(c), counter(cnt), child(ch)
        {
        }

        void execute() override
        {
            counter.fetch_add(1, std::memory_order_relaxed);
            if (child) {
                ctx.post(*child);
            }
        }
    };

    SelfPoster second(ctx, counter, nullptr);
    SelfPoster first(ctx, counter, &second);

    zenrts::WorkGuard guard(ctx);
    std::thread worker([&ctx] { ctx.run(); });

    ctx.post(first);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(counter.load(), 2);

    ctx.stop();
    worker.join();
}
