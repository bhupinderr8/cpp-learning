#include "cache_bench.hpp"
#include <iostream>

int main() {
    using namespace bench;

    std::cout << "=== Weekend Lab 1 Scaffolding Check ===\n";
    std::cout << "Hardware interference size : " << hardware_destructive_interference_size << " bytes\n";
    std::cout << "sizeof(PackedCounters)      : " << sizeof(PackedCounters) << " bytes\n";
    std::cout << "sizeof(IsolatedCounters)    : " << sizeof(IsolatedCounters) << " bytes\n";
    std::cout << "sizeof(Record)              : " << sizeof(Record) << " bytes\n";

    // Verify layout invariants
    static_assert(sizeof(PackedCounters) == bench::NUM_THREADS * sizeof(std::atomic<uint64_t>),
                  "PackedCounters has unexpected layout/padding!");
    static_assert(alignof(PaddedCounter) >= hardware_destructive_interference_size,
                  "PaddedCounter must align to cache line boundary!");

    std::cout << "Scaffolding compiled successfully. Ready for Saturday Build.\n";
    return 0;
}