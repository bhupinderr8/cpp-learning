#include "aligned_arena.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <thread>

class AlignedArenaBench {
public:
    int run() {
        AlignedArena arena;
        std::array<std::thread, WorkerCount> workers;
        std::array<std::uint64_t, WorkerCount> checksums{};

        const auto start = std::chrono::steady_clock::now();
        for (std::size_t worker = 0; worker < WorkerCount; ++worker) {
            workers[worker] = std::thread([&arena, &checksums, worker] {
                checksums[worker] = runWorker(arena, worker);
            });
        }
        for (std::thread& worker : workers) {
            worker.join();
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        const std::uint64_t checksum =
            std::accumulate(checksums.begin(), checksums.end(), std::uint64_t{0});
        std::cout << "Workers: " << WorkerCount << '\n'
                  << "Allocation/deallocation calls: " << TotalOperations << '\n'
                  << "Elapsed: " << elapsed.count() << " ms\n"
                  << "Checksum: " << checksum << '\n'
                  << "Arena bytes reserved: " << arena.bytesUsed() << '\n';
        return checksum == 0 ? 1 : 0;
    }

private:
    static constexpr std::size_t WorkerCount = 8;
    static constexpr std::size_t IterationsPerWorker = 625'000;
    static constexpr std::size_t TotalOperations =
        WorkerCount * IterationsPerWorker * 2;

    static std::uint64_t runWorker(AlignedArena& arena, std::size_t workerIndex) {
        std::uint64_t checksum = 0;
        for (std::size_t i = 0; i < IterationsPerWorker; ++i) {
            const unsigned classOffset =
                static_cast<unsigned>((i + workerIndex) % AlignedArena::SizeClassCount);
            const std::size_t size =
                AlignedArena::MinAllocationSize << classOffset;
            auto* memory = static_cast<std::byte*>(arena.allocate(size));
            memory[0] = static_cast<std::byte>(i + workerIndex);
            memory[size - 1] = static_cast<std::byte>(size);
            checksum += std::to_integer<unsigned char>(memory[0]);
            arena.deallocate(memory, size);
        }
        return checksum;
    }
};

int main() {
    AlignedArenaBench benchmark;
    return benchmark.run();
}
