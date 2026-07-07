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
#include <zenrts/strand.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace {

struct TestTask : zenrts::StrandTaskBase {
    std::vector<int>& order;
    std::mutex& mtx;
    int id;

    TestTask(std::vector<int>& o, std::mutex& m, int i)
        : order(o), mtx(m), id(i)
    {
    }

    void on_execute() override
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

TEST(StrandTest, TasksExecuteInOrder)
{
    zenrts::Context ctx;
    zenrts::Strand strand(ctx);
    zenrts::WorkGuard guard(ctx);

    std::vector<int> order;
    std::mutex mtx;
    TestTask t1(order, mtx, 1);
    TestTask t2(order, mtx, 2);
    TestTask t3(order, mtx, 3);

    t1.set_strand(strand);
    t2.set_strand(strand);
    t3.set_strand(strand);

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    strand.post(t1);
    strand.post(t2);
    strand.post(t3);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    {
        std::lock_guard lock(mtx);
        EXPECT_EQ(order.size(), 3);
        if (order.size() >= 3) {
            EXPECT_EQ(order[0], 1);
            EXPECT_EQ(order[1], 2);
            EXPECT_EQ(order[2], 3);
        }
    }

    ctx.stop();
}

TEST(StrandTest, TasksFromMultipleThreads)
{
    zenrts::Context ctx;
    zenrts::Strand strand(ctx);
    zenrts::WorkGuard guard(ctx);

    std::atomic<int> counter{0};

    struct CounterTask : zenrts::StrandTaskBase {
        std::atomic<int>& counter;

        CounterTask(std::atomic<int>& c)
            : counter(c)
        {
        }

        void on_execute() override
        {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    };

    CounterTask t1(counter);
    CounterTask t2(counter);
    CounterTask t3(counter);

    t1.set_strand(strand);
    t2.set_strand(strand);
    t3.set_strand(strand);

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    std::thread poster1([&] { strand.post(t1); });
    std::thread poster2([&] { strand.post(t2); });
    std::thread poster3([&] { strand.post(t3); });

    poster1.join();
    poster2.join();
    poster3.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(counter.load(), 3);

    ctx.stop();
}

TEST(StrandTest, SingleTask)
{
    zenrts::Context ctx;
    zenrts::Strand strand(ctx);
    zenrts::WorkGuard guard(ctx);

    bool executed = false;
    std::mutex mtx;
    std::condition_variable cv;

    struct NotifyTask : zenrts::StrandTaskBase {
        bool& executed;
        std::mutex& mtx;
        std::condition_variable& cv;

        NotifyTask(bool& e, std::mutex& m, std::condition_variable& c)
            : executed(e), mtx(m), cv(c)
        {
        }

        void on_execute() override
        {
            {
                std::lock_guard lock(mtx);
                executed = true;
            }
            cv.notify_one();
        }
    };

    NotifyTask task(executed, mtx, cv);
    task.set_strand(strand);

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    strand.post(task);

    {
        std::unique_lock lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(2), [&] { return executed; });
    }

    EXPECT_TRUE(executed);

    ctx.stop();
}

TEST(StrandTest, StrandWithoutContextPost)
{
    zenrts::Context ctx;
    zenrts::Strand strand(ctx);

    std::vector<int> order;
    std::mutex mtx;
    TestTask t1(order, mtx, 1);

    t1.set_strand(strand);

    ctx.post(t1);

    std::thread worker([&ctx] { ctx.run(); });
    ThreadGuard g(worker);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    {
        std::lock_guard lock(mtx);
        EXPECT_EQ(order.size(), 1);
        if (order.size() >= 1) {
            EXPECT_EQ(order[0], 1);
        }
    }

    ctx.stop();
}
