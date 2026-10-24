#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <new>
#include <vector>

// ---------------------------------------------------------------------------
// 1. Hardware Cache Line Constants
// ---------------------------------------------------------------------------
#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_destructive_interference_size;
#else
    // 64 bytes is the standard L1 cache line size on modern x86_64
    constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

namespace bench {

constexpr std::size_t NUM_THREADS = 8;
constexpr uint64_t ITERATIONS_PER_THREAD = 50'000'000ULL;
constexpr std::size_t NUM_RECORDS = 20'000'000ULL;

// Helper to prevent the compiler from eliding dead code in benchmark loops
template <typename T>
inline void do_not_optimize(T const& value) {
    asm volatile("" : : "r,m"(value) : "memory");
}

// ---------------------------------------------------------------------------
// 2. Part 1: False Sharing Structures
// ---------------------------------------------------------------------------

// BAD: 8 adjacent 8-byte atomic counters (8 * 8B = 64B).
// All fit into a single 64B cache line, inducing heavy MESI RFO invalidations.
struct PackedCounters {
    std::atomic<uint64_t> counters[NUM_THREADS]{};
};

// GOOD: Each counter is padded/aligned to 64 bytes.
// Each worker thread operates on its own private cache line with 0 invalidations.
struct alignas(hardware_destructive_interference_size) PaddedCounter {
    std::atomic<uint64_t> value{0};
};

struct IsolatedCounters {
    PaddedCounter counters[NUM_THREADS];
};

// ---------------------------------------------------------------------------
// 3. Part 2: AoS vs SoA Structures (20M Record Scan)
// ---------------------------------------------------------------------------

// 32-byte tuple: price is scanned, but id/timestamp/flags pollute cache lines
struct Record {
    uint64_t id;
    double   price;      // Column to aggregate
    uint32_t volume;
    uint32_t flags;
    uint64_t timestamp;
};

// Array of Structs
struct AoSContainer {
    std::vector<Record> records;
    explicit AoSContainer(std::size_t size) : records(size) {}
};

// Struct of Arrays: prices vector holds 8 contiguous doubles per 64B cache line
struct SoAContainer {
    std::vector<uint64_t> ids;
    std::vector<double>   prices;
    std::vector<uint32_t> volumes;
    std::vector<uint32_t> flags;
    std::vector<uint64_t> timestamps;

    explicit SoAContainer(std::size_t size)
        : ids(size), prices(size), volumes(size), flags(size), timestamps(size) {}
};

} // namespace bench