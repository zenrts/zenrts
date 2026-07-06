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

#include <benchmark/benchmark.h>
#include <zenrts/context.h>
#include <zenrts/deadline_timer.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace {

struct Signal {
    std::atomic<int> count{0};
    std::mutex mtx;
    std::condition_variable cv;

    void wait(int n)
    {
        std::unique_lock lock(mtx);
        cv.wait(lock, [this, n] { return count.load(std::memory_order_relaxed) >= n; });
    }

    void bump()
    {
        count.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(mtx);
        cv.notify_one();
    }

    void reset() { count.store(0, std::memory_order_relaxed); }
};

struct CountingTask : zenrts::TaskBase {
    Signal& sig;

    explicit CountingTask(Signal& s)
        : sig(s)
    {
    }

    void execute() override
    {
        sig.bump();
    }
};

struct CountingTimer : zenrts::DeadlineTimer {
    Signal& sig;

    CountingTimer(zenrts::Context& ctx, Signal& s)
        : DeadlineTimer(ctx), sig(s)
    {
    }

    void on_timer(std::error_code) override
    {
        sig.bump();
    }
};

struct CancelTimer : zenrts::DeadlineTimer {
    Signal& sig;

    CancelTimer(zenrts::Context& ctx, Signal& s)
        : DeadlineTimer(ctx), sig(s)
    {
    }

    void on_timer(std::error_code ec) override
    {
        if (ec == std::make_error_code(std::errc::operation_canceled)) {
            sig.bump();
        }
    }
};

} // namespace

static void BM_PostThroughput(benchmark::State& state)
{
    const int batch = state.range(0);

    zenrts::Context ctx;
    zenrts::WorkGuard guard(ctx);
    std::thread worker([&ctx] { ctx.run(); });

    Signal sig;
    auto** tasks = new CountingTask*[batch];
    for (int i = 0; i < batch; ++i) {
        tasks[i] = new CountingTask(sig);
    }

    for (auto _ : state) {
        sig.reset();

        for (int i = 0; i < batch; ++i) {
            ctx.post(*tasks[i]);
        }

        sig.wait(batch);
    }

    state.SetItemsProcessed(state.iterations() * batch);

    for (int i = 0; i < batch; ++i) delete tasks[i];
    delete[] tasks;

    ctx.stop();
    worker.join();
}
BENCHMARK(BM_PostThroughput)
    ->RangeMultiplier(2)
    ->Range(8, 8 << 10)
    ->Unit(benchmark::kMicrosecond);

static void BM_TaskLatency(benchmark::State& state)
{
    zenrts::Context ctx;
    zenrts::WorkGuard guard(ctx);

    std::thread worker([&ctx] { ctx.run(); });

    Signal sig;
    CountingTask task(sig);

    ctx.post(task);
    sig.wait(1);
    sig.reset();

    for (auto _ : state) {
        sig.reset();
        ctx.post(task);
        sig.wait(1);
    }

    ctx.stop();
    worker.join();
}
BENCHMARK(BM_TaskLatency)->Unit(benchmark::kMicrosecond);

static void BM_TimerWaitThroughput(benchmark::State& state)
{
    const int batch = state.range(0);

    zenrts::Context ctx;
    zenrts::WorkGuard guard(ctx);
    std::thread worker([&ctx] { ctx.run(); });

    Signal sig;
    auto** timers = new CountingTimer*[batch];
    for (int i = 0; i < batch; ++i) {
        timers[i] = new CountingTimer(ctx, sig);
    }

    for (auto _ : state) {
        sig.reset();

        for (int i = 0; i < batch; ++i) {
            timers[i]->expires_at(std::chrono::steady_clock::now());
            timers[i]->async_wait();
        }

        sig.wait(batch);
    }

    state.SetItemsProcessed(state.iterations() * batch);

    for (int i = 0; i < batch; ++i) delete timers[i];
    delete[] timers;

    ctx.stop();
    worker.join();
}
BENCHMARK(BM_TimerWaitThroughput)
    ->RangeMultiplier(2)
    ->Range(8, 8 << 10)
    ->Unit(benchmark::kMicrosecond);

static void BM_CancelThroughput(benchmark::State& state)
{
    const int batch = state.range(0);

    zenrts::Context ctx;
    zenrts::WorkGuard guard(ctx);
    std::thread worker([&ctx] { ctx.run(); });

    Signal sig;
    auto** timers = new CancelTimer*[batch];
    for (int i = 0; i < batch; ++i) {
        timers[i] = new CancelTimer(ctx, sig);
    }

    for (auto _ : state) {
        sig.reset();

        for (int i = 0; i < batch; ++i) {
            timers[i]->expires_after(std::chrono::hours(1));
            timers[i]->async_wait();
        }
        for (int i = 0; i < batch; ++i) {
            timers[i]->cancel();
        }

        sig.wait(batch);
    }

    state.SetItemsProcessed(state.iterations() * batch);

    for (int i = 0; i < batch; ++i) delete timers[i];
    delete[] timers;

    ctx.stop();
    worker.join();
}
BENCHMARK(BM_CancelThroughput)
    ->RangeMultiplier(2)
    ->Range(8, 8 << 10)
    ->Unit(benchmark::kMicrosecond);

static void BM_PostMultiThread(benchmark::State& state)
{
    const int num_threads = state.range(0);
    const int batch = state.range(1);

    zenrts::Context ctx;
    zenrts::WorkGuard guard(ctx);
    std::thread worker([&ctx] { ctx.run(); });

    Signal sig;

    auto** tasks = new CountingTask*[num_threads * batch];
    for (int i = 0; i < num_threads * batch; ++i) {
        tasks[i] = new CountingTask(sig);
    }

    for (int i = 0; i < num_threads * batch; ++i) ctx.post(*tasks[i]);
    sig.wait(num_threads * batch);
    sig.reset();

    for (auto _ : state) {
        std::vector<std::thread> posters;
        for (int t = 0; t < num_threads; ++t) {
            posters.emplace_back([&, t] {
                auto start = t * batch;
                for (int i = 0; i < batch; ++i) {
                    ctx.post(*tasks[start + i]);
                }
            });
        }

        state.PauseTiming();
        for (auto& th : posters) {
            th.join();
        }
        sig.wait(num_threads * batch);
        sig.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * num_threads * batch);
    state.counters["threads"] = num_threads;

    for (int i = 0; i < num_threads * batch; ++i) delete tasks[i];
    delete[] tasks;

    ctx.stop();
    worker.join();
}
BENCHMARK(BM_PostMultiThread)
    ->Args({1, 256})
    ->Args({2, 256})
    ->Args({4, 256})
    ->Args({8, 256})
    ->Args({16, 256})
    ->Args({24, 256})
    ->Unit(benchmark::kMicrosecond);

static void BM_CancelMultiThread(benchmark::State& state)
{
    const int num_threads = state.range(0);
    const int batch = state.range(1);

    zenrts::Context ctx;
    zenrts::WorkGuard guard(ctx);
    std::thread worker([&ctx] { ctx.run(); });

    Signal sig;

    auto** timers = new CancelTimer*[num_threads * batch];
    for (int i = 0; i < num_threads * batch; ++i) {
        timers[i] = new CancelTimer(ctx, sig);
    }

    for (int i = 0; i < num_threads * batch; ++i) {
        timers[i]->expires_after(std::chrono::hours(1));
        timers[i]->async_wait();
    }
    for (int i = 0; i < num_threads * batch; ++i) timers[i]->cancel();
    sig.wait(num_threads * batch);
    sig.reset();

    for (auto _ : state) {
        for (int i = 0; i < num_threads * batch; ++i) {
            timers[i]->expires_after(std::chrono::hours(1));
            timers[i]->async_wait();
        }

        std::vector<std::thread> posters;
        for (int t = 0; t < num_threads; ++t) {
            posters.emplace_back([&, t] {
                auto start = t * batch;
                for (int i = 0; i < batch; ++i) {
                    timers[start + i]->cancel();
                }
            });
        }

        state.PauseTiming();
        for (auto& th : posters) {
            th.join();
        }
        sig.wait(num_threads * batch);
        sig.reset();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * num_threads * batch);
    state.counters["threads"] = num_threads;

    for (int i = 0; i < num_threads * batch; ++i) delete timers[i];
    delete[] timers;

    ctx.stop();
    worker.join();
}
BENCHMARK(BM_CancelMultiThread)
    ->Args({1, 256})
    ->Args({2, 256})
    ->Args({4, 256})
    ->Args({8, 256})
    ->Args({16, 256})
    ->Args({24, 256})
    ->Unit(benchmark::kMicrosecond);
