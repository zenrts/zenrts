add_executable(benchmark_zenrts
    benchmark_core.cpp
)

target_link_libraries(benchmark_zenrts PRIVATE zenrts benchmark::benchmark)
