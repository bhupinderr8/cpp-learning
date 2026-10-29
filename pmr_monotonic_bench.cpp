#include <iostream>
#include <chrono>
#include <memory_resource>
#include <list>
#include <memory>

constexpr size_t NUM_NODES = 200'000;

// -----------------------------------------------------------------------------
// Benchmark 1: Standard Heap Node Allocation (200,000 individual malloc calls)
// -----------------------------------------------------------------------------
void bench_standard_list() {
    auto t0 = std::chrono::steady_clock::now();

    {
        // Each node in std::list is allocated independently via malloc
        std::list<int> node_list;
        for (size_t i = 0; i < NUM_NODES; ++i) {
            node_list.push_back(static_cast<int>(i));
        }
        // Destruction traverses all 200,000 nodes, calling free() one by one!
    }

    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::cout << "[Standard std::list (Heap malloc/free)] Total: " << us << " us\n";
}

// -----------------------------------------------------------------------------
// Benchmark 2: PMR Monotonic Buffer Resource Node Allocation
// -----------------------------------------------------------------------------
void bench_pmr_list() {
    // 16 MB buffer: enough to hold 200,000 list nodes (node = 24 bytes + alignment)
    constexpr size_t POOL_SIZE = 16 * 1024 * 1024;
    auto backing_buffer = std::make_unique<std::byte[]>(POOL_SIZE);

    auto t0 = std::chrono::steady_clock::now();

    {
        std::pmr::monotonic_buffer_resource arena(backing_buffer.get(), POOL_SIZE);
        std::pmr::list<int> pmr_list(&arena);

        for (size_t i = 0; i < NUM_NODES; ++i) {
            pmr_list.push_back(static_cast<int>(i));
        }
        // Destruction: monotonic arena ignores individual deallocations.
        // Memory winks out instantly when arena scope ends!
    }

    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::cout << "[PMR Monotonic std::pmr::list]        Total: " << us << " us\n";
}

int main() {
    std::cout << "=== Node-Based Allocation Benchmark (200k Nodes) ===\n\n";
    bench_standard_list();
    bench_pmr_list();
    return 0;
}