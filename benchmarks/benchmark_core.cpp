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
#include <zenrts/core.h>
#include <cstdlib>
#include <csignal>
#include <iostream>
#include <execinfo.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <cstring>
#include <unistd.h>

static void hang_handler(int sig)
{
    std::cerr << "\n=== BENCHMARK TIMEOUT (60s) ===\n";
    std::cerr << "Backtrace of current thread (tid=" << gettid() << "):\n";

    void* buffer[128];
    int frames = backtrace(buffer, 128);

    for (int i = 0; i < frames; ++i) {
        Dl_info info;
        if (dladdr(buffer[i], &info)) {
            int status = 0;
            char* demangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
            const char* name = demangled ? demangled : info.dli_sname;
            std::cerr << "  #" << i << " " << name << " +" << (uintptr_t)buffer[i] - (uintptr_t)info.dli_saddr << "\n";
            free(demangled);
        } else {
            std::cerr << "  #" << i << " <unknown>\n";
        }
    }

    std::cerr << "===============================\n";
    abort();
}

static void setup_hang_detection()
{
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = hang_handler;
    sa.sa_flags = SA_RESTART | SA_NODEFER;
    sigaction(SIGALRM, &sa, nullptr);
    alarm(120);
}

static void BM_Version(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto v = zenrts::version();
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_Version);

int main(int argc, char** argv)
{
    setup_hang_detection();
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    alarm(0);
    return 0;
}
