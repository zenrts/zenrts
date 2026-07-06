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
#include <zenrts/strand.h>
#include <atomic>
#include <thread>
#include <vector>

namespace {

struct CountingStrandTask : zenrts::StrandTaskBase {
    std::atomic<int>& counter;

    explicit CountingStrandTask(std::atomic<int>& c)
        : counter(c)
    {
    }

    void on_execute() override
    {
        counter.fetch_add(1, std::memory_order_relaxed);
    }
};

void wait_for(std::atomic<int>& counter, int target)
{
    while (counter.load(std::memory_order_relaxed) != target) {
        std::this_thread::yield();
    }
}

} // namespace

// ── Strand serial throughput ──

static void BM_StrandSerialThroughput(benchmark::State& state)
{
    const int batch = state.range(0);

    zenrts::Context ctx;
    zenrts::Strand strand(ctx);
    zenrts::WorkGuard guard(ctx);

    std::thread worker([&ctx] { ctx.run(); });

    std::atomic<int> counter{0};

    auto** tasks = new CountingStrandTask*[batch];
    for (int i = 0; i < batch; ++i) {
        tasks[i] = new CountingStrandTask(counter);
        tasks[i]->set_strand(strand);
    }

    for (auto _ : state) {
        counter.store(0, std::memory_order_relaxed);

        for (int i = 0; i < batch; ++i) {
            strand.post(*tasks[i]);
        }

        wait_for(counter, batch);
    }

    state.SetItemsProcessed(state.iterations() * batch);

    for (int i = 0; i < batch; ++i) delete tasks[i];
    delete[] tasks;

    ctx.stop();
    worker.join();
}
BENCHMARK(BM_StrandSerialThroughput)
    ->RangeMultiplier(2)
    ->Range(8, 8 << 10)
    ->Unit(benchmark::kMicrosecond);

// ── Strand multi-threaded post throughput ──

static void BM_StrandMultiThread(benchmark::State& state)
{
    const int num_threads = state.range(0);
    const int batch = state.range(1);

    zenrts::Context ctx;
    zenrts::Strand strand(ctx);
    zenrts::WorkGuard guard(ctx);

    std::thread worker([&ctx] { ctx.run(); });

    std::atomic<int> counter{0};

    auto** tasks = new CountingStrandTask*[num_threads * batch];
    for (int i = 0; i < num_threads * batch; ++i) {
        tasks[i] = new CountingStrandTask(counter);
        tasks[i]->set_strand(strand);
    }

    // Warm-up
    for (int i = 0; i < num_threads * batch; ++i) strand.post(*tasks[i]);
    wait_for(counter, num_threads * batch);
    counter.store(0, std::memory_order_relaxed);

    for (auto _ : state) {
        std::vector<std::thread> posters;
        for (int t = 0; t < num_threads; ++t) {
            posters.emplace_back([&, t] {
                auto start = t * batch;
                for (int i = 0; i < batch; ++i) {
                    strand.post(*tasks[start + i]);
                }
            });
        }

        state.PauseTiming();
        for (auto& th : posters) {
            th.join();
        }
        wait_for(counter, num_threads * batch);
        counter.store(0, std::memory_order_relaxed);
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * num_threads * batch);
    state.counters["threads"] = num_threads;

    for (int i = 0; i < num_threads * batch; ++i) delete tasks[i];
    delete[] tasks;

    ctx.stop();
    worker.join();
}
BENCHMARK(BM_StrandMultiThread)
    ->Args({1, 256})
    ->Args({2, 256})
    ->Args({4, 256})
    ->Args({8, 256})
    ->Args({16, 256})
    ->Args({24, 256})
    ->Unit(benchmark::kMicrosecond);
