/*
To compile - clang++ -std=c++20 -Wall -Wextra sketch_alignment.cpp -o sketch_alignment
./sketch_alignment
*/
#include <iostream>
#include <cstdint>
#include <cassert>
#include <vector>

// -----------------------------------------------------------------------------
// Part 1: Branchless Bitwise Alignment Math
// -----------------------------------------------------------------------------
inline uintptr_t align_forward(uintptr_t ptr, size_t alignment) {
    // Invariant: alignment must be a non-zero power of 2 (e.g., 8, 16, 64)
    assert((alignment & (alignment - 1)) == 0 && "Alignment must be a power of 2");

    // TODO: Return ptr rounded UP to the nearest multiple of alignment.
    // Formula hint: add the slack, then bitwise-AND with the inverted mask.
    return (ptr + (alignment - 1)) & ~(alignment - 1);
}

inline void* align_forward(void* ptr, size_t alignment) {
    return reinterpret_cast<void*>(
        align_forward(reinterpret_cast<uintptr_t>(ptr), alignment)
    );
}

// -----------------------------------------------------------------------------
// Part 2: Minimal Monotonic Bump Arena
// -----------------------------------------------------------------------------
class SimpleBumpArena {
public:
    explicit SimpleBumpArena(size_t capacity)
        : capacity_(capacity), offset_(0) {
        // Allocate raw backing storage from the OS
        buffer_ = new uint8_t[capacity_];
    }

    ~SimpleBumpArena() {
        delete[] buffer_;
    }

    // Disable copying
    SimpleBumpArena(const SimpleBumpArena&) = delete;
    SimpleBumpArena& operator=(const SimpleBumpArena&) = delete;

    void* allocate(size_t bytes, size_t alignment) {
        // 1. Calculate current unaligned virtual address
        uintptr_t current_addr = reinterpret_cast<uintptr_t>(buffer_) + offset_;

        // 2. Snap address forward to meet hardware alignment
        uintptr_t aligned_addr = align_forward(current_addr, alignment);

        // TODO: Compute the updated offset from buffer_ start after placing 'bytes'
        size_t new_offset = aligned_addr - reinterpret_cast<uintptr_t>(buffer_) + bytes;

        // 3. Out-of-memory check
        if (new_offset > capacity_) {
            std::cerr << "[Arena] Out of memory!\n";
            return nullptr;
        }

        // TODO: Advance offset_ to new_offset and return the aligned pointer
        offset_ = new_offset;
        return reinterpret_cast<void*>(aligned_addr);
    }

    void reset() {
        // Lakos "Winking Out": reclaim all memory in O(1)
        // TODO: Reset the arena so subsequent allocations start from the beginning
        offset_ = 0;
    }

    size_t bytes_used() const { return offset_; }
    size_t capacity() const { return capacity_; }

private:
    uint8_t* buffer_{nullptr};
    size_t capacity_{0};
    size_t offset_{0};
};

// -----------------------------------------------------------------------------
// Test Harness & Assertions
// -----------------------------------------------------------------------------
int main() {
    std::cout << "=== Week 2 Day 1: Alignment & Bump Pointer Check ===\n\n";

    // 1. Synthetic test cases for bitwise alignment math
    struct TestCase {
        uintptr_t input_addr;
        size_t alignment;
        uintptr_t expected;
    };

    std::vector<TestCase> cases = {
        {100, 64, 128}, // 100 rounds up to 128
        {128, 64, 128}, // Already aligned: must not advance
        {129, 64, 192}, // 129 rounds up to next multiple (192)
        {1,    8,   8}, // 1 rounds up to 8
        {8,    8,   8}, // 8 stays 8
        {9,   16,  16}, // 9 rounds up to 16
    };

    for (const auto& tc : cases) {
        uintptr_t result = align_forward(tc.input_addr, tc.alignment);
        std::cout << "Addr: " << tc.input_addr << " | Align: " << tc.alignment
                  << " -> Aligned: " << result << "\n";
        assert(result == tc.expected);
        assert(result % tc.alignment == 0);
    }
    std::cout << ">> All synthetic math checks passed!\n\n";

    // 2. Test allocations and alignment invariants from the arena
    SimpleBumpArena arena(1024 * 1024); // 1 MB buffer

    void* p1 = arena.allocate(13, 8);   // 13 bytes, 8-byte aligned
    void* p2 = arena.allocate(7, 64);   // 7 bytes, 64-byte aligned (cache line boundary)
    void* p3 = arena.allocate(32, 16);  // 32 bytes, 16-byte aligned

    assert(p1 != nullptr && reinterpret_cast<uintptr_t>(p1) % 8 == 0);
    assert(p2 != nullptr && reinterpret_cast<uintptr_t>(p2) % 64 == 0);
    assert(p3 != nullptr && reinterpret_cast<uintptr_t>(p3) % 16 == 0);

    std::cout << "Alloc 1 (align  8): " << p1 << "\n";
    std::cout << "Alloc 2 (align 64): " << p2 << "\n";
    std::cout << "Alloc 3 (align 16): " << p3 << "\n";
    std::cout << "Total bytes used (with alignment padding): " << arena.bytes_used() << "\n";

    // 3. Test O(1) reset
    arena.reset();
    assert(arena.bytes_used() == 0);
    std::cout << ">> Arena reset successfully verified!\n";

    return 0;
}