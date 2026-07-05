#include <benchmark/benchmark.h>
#include <zenrts/core.h>

static void BM_Version(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto v = zenrts::version();
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_Version);

BENCHMARK_MAIN();
