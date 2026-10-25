#pragma once

#include <atomic>
#include <cstdint>
#include <new>
#include <vector>

namespace bench {

constexpr size_t NUM_THREADS = 8;
constexpr size_t ITERATIONS_PER_THREAD = 50'000'000;
constexpr size_t NUM_RECORDS = 20'000'000;

// Standard 64-byte x86 cache line fallback if interference size is not supported
#ifdef __cpp_lib_hardware_interference_size
using std::hardware_destructive_interference_size;
#else
constexpr size_t hardware_destructive_interference_size = 64;
#endif

// -----------------------------------------------------------------------------
// Part 1: False Sharing Types
// -----------------------------------------------------------------------------
// Packed: 8 uint64 counters (64 bytes total) residing on the exact same L1 cache line
struct PackedCounters {
    std::atomic<uint64_t> counters[NUM_THREADS];
};

// Padded: each counter isolated on its own distinct 64-byte boundary
struct alignas(hardware_destructive_interference_size) PaddedSlot {
    std::atomic<uint64_t> value{0};
};

struct PaddedCounters {
    PaddedSlot counters[NUM_THREADS];
};

// -----------------------------------------------------------------------------
// Part 2: AoS vs. SoA Types (20M Records)
// -----------------------------------------------------------------------------
// AoS: 32 bytes per record. Scanning active_flag fetches 24 bytes of dead payload
struct RecordAoS {
    uint32_t id;
    uint32_t active_flag;
    double payload1;
    double payload2;
};

// SoA: Contiguous parallel vectors. active_flags vector has 100% cache-line utilization
struct RecordsSoA {
    std::vector<uint32_t> ids;
    std::vector<uint32_t> active_flags;
    std::vector<double> payloads1;
    std::vector<double> payloads2;

    explicit RecordsSoA(size_t n)
        : ids(n, 0), active_flags(n, 1), payloads1(n, 0.0), payloads2(n, 0.0) {}
};

} // namespace bench