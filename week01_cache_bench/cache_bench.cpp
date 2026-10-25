#include "cache_bench.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

void run_false_sharing_benchmark() {
    std::cout << "========================================\n";
    std::cout << " 1. Multicore False Sharing Benchmark   \n";
    std::cout << "========================================\n";

    // 1. Packed counters: heavy cross-core MESI invalidation (RFO) ping-pong
    bench::PackedCounters packed{};
    auto t0 = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        threads.reserve(bench::NUM_THREADS);
        for (size_t t = 0; t < bench::NUM_THREADS; ++t) {
            threads.emplace_back([&packed, t]() {
                for (size_t i = 0; i < bench::ITERATIONS_PER_THREAD; ++i) {
                    packed.counters[t].fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    auto packed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::cout << "[Packed Counters] Duration : " << packed_ms << " ms\n";

    // 2. Padded counters: zero cache line contention between cores
    bench::PaddedCounters padded{};
    auto t2 = std::chrono::steady_clock::now();
    {
        std::vector<std::jthread> threads;
        threads.reserve(bench::NUM_THREADS);
        for (size_t t = 0; t < bench::NUM_THREADS; ++t) {
            threads.emplace_back([&padded, t]() {
                for (size_t i = 0; i < bench::ITERATIONS_PER_THREAD; ++i) {
                    padded.counters[t].value.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
    }
    auto t3 = std::chrono::steady_clock::now();
    auto padded_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
    std::cout << "[Padded Counters] Duration : " << padded_ms << " ms\n";
    if (padded_ms > 0) {
        std::cout << "--> Speedup by eliminating false sharing: " 
                  << static_cast<double>(packed_ms) / padded_ms << "x\n\n";
    }
}

void run_layout_benchmark() {
    std::cout << "========================================\n";
    std::cout << " 2. AoS vs. SoA Scan (20M Records)      \n";
    std::cout << "========================================\n";

    // 1. AoS Sequential Scan
    std::vector<bench::RecordAoS> aos(bench::NUM_RECORDS, {1, 1, 2.0, 3.0});
    auto t0 = std::chrono::steady_clock::now();
    uint64_t aos_sum = 0;
    for (size_t i = 0; i < bench::NUM_RECORDS; ++i) {
        aos_sum += aos[i].active_flag;
    }
    auto t1 = std::chrono::steady_clock::now();
    auto aos_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    std::cout << "[AoS Scan] (sum=" << aos_sum << ") : " << aos_ms << " ms\n";

    // 2. SoA Sequential Scan
    bench::RecordsSoA soa(bench::NUM_RECORDS);
    auto t2 = std::chrono::steady_clock::now();
    uint64_t soa_sum = 0;
    const size_t n = soa.active_flags.size();
    for (size_t i = 0; i < n; ++i) {
        soa_sum += soa.active_flags[i];
    }
    auto t3 = std::chrono::steady_clock::now();
    auto soa_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();
    std::cout << "[SoA Scan] (sum=" << soa_sum << ") : " << soa_ms << " ms\n";
    if (soa_ms > 0) {
        std::cout << "--> Speedup using data-oriented SoA layout: " 
                  << static_cast<double>(aos_ms) / soa_ms << "x\n";
    }
}

int main() {
    run_false_sharing_benchmark();
    run_layout_benchmark();
    return 0;
}