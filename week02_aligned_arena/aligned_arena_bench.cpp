#include "aligned_arena.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <new>
#include <vector>

class AlignedArenaBench {
public:
    int run() {
        runArenaBenchmark();
        runHeapBenchmark();
        return 0;
    }

private:
    struct Allocation {
        void* pointer;
        std::size_t size;
        std::size_t alignment;
    };

    static constexpr std::array<std::size_t, 5> AllocationSizes{
        16, 64, 256, 1024, 4096};
    static constexpr std::size_t OperationsPerBatch = 10'000;
    static constexpr std::size_t BatchCount = 8;

    static std::vector<Allocation> allocateBatch(AlignedArena& arena) {
        std::vector<Allocation> allocations;
        allocations.reserve(OperationsPerBatch);

        for (std::size_t i = 0; i < OperationsPerBatch; ++i) {
            const std::size_t size = AllocationSizes[i % AllocationSizes.size()];
            const std::size_t alignment = size < 256 ? 16 : 256;
            void* pointer = arena.allocate(size, alignment);
            static_cast<std::byte*>(pointer)[0] = static_cast<std::byte>(i);
            allocations.push_back({pointer, size, alignment});
        }
        return allocations;
    }

    static void releaseBatch(AlignedArena& arena,
                             const std::vector<Allocation>& allocations) {
        for (const Allocation& allocation : allocations) {
            arena.deallocate(allocation.pointer, allocation.size);
        }
    }

    static std::vector<Allocation> allocateHeapBatch() {
        std::vector<Allocation> allocations;
        allocations.reserve(OperationsPerBatch);

        for (std::size_t i = 0; i < OperationsPerBatch; ++i) {
            const std::size_t size = AllocationSizes[i % AllocationSizes.size()];
            const std::size_t alignment = size < 256 ? 16 : 256;
            void* pointer =
                ::operator new(size, std::align_val_t{alignment});
            static_cast<std::byte*>(pointer)[0] = static_cast<std::byte>(i);
            allocations.push_back({pointer, size, alignment});
        }
        return allocations;
    }

    static void releaseHeapBatch(const std::vector<Allocation>& allocations) {
        for (const Allocation& allocation : allocations) {
            ::operator delete(allocation.pointer,
                              std::align_val_t{allocation.alignment});
        }
    }

    static void runArenaBenchmark() {
        AlignedArena arena;
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t batch = 0; batch < BatchCount; ++batch) {
            const auto allocations = allocateBatch(arena);
            releaseBatch(arena, allocations);
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start);
        printResult("AlignedArena (bump + thread-local freelist)", elapsed.count());
    }

    static void runHeapBenchmark() {
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t batch = 0; batch < BatchCount; ++batch) {
            const auto allocations = allocateHeapBatch();
            releaseHeapBatch(allocations);
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start);
        printResult("Aligned heap new/delete", elapsed.count());
    }

    static void printResult(const char* label, long long elapsedMicroseconds) {
        const std::size_t totalOperations = OperationsPerBatch * BatchCount;
        std::cout << '[' << label << "] " << elapsedMicroseconds << " us for "
                  << totalOperations << " allocations and deallocations ("
                  << static_cast<double>(elapsedMicroseconds) / totalOperations
                  << " us/op)\n";
    }
};

int main() {
    AlignedArenaBench benchmark;
    return benchmark.run();
}
