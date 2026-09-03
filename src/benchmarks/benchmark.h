#pragma once

#include <cstdint>
#include <cstddef>

struct BenchmarkResult {
    const char *name;
    uint64_t cycles;
    double latency_us;
    uint32_t throughput;
    size_t ram_used;
};

class Benchmark {
public:
    void run_all();
    void run_logic();
    void run_counter();
    void run_lfsr();
    void run_nn();
    void run_profile();
    void run_fabric_suite();
};
