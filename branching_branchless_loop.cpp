/*
use this to compile - clang++ -std=c++20 -O2 -g branching_branchless_loop.cpp -o branch_bench
-02 is the optimization level, -g is for debugging symbols, and -std=c++20 specifies the C++ standard version to use.


This is for testing the performance - sudo perf stat -e cycles,instructions,branches,branch-misses ./branch_bench branch_random

perf stat is a Linux command that collects and displays performance statistics for a given command.
In this case, it is used to measure the performance of the branch_bench program with the branch_random mode.

This is used to see the assembly of the compiled machine code - objdump -d -C branch_bench | grep -A 25 "<sum_branching"
*/

#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <string>

template <typename T>
inline void do_not_optimize(T&& val) {
    asm volatile("" : : "g"(val) : "memory");
}

uint64_t sum_branching(const std::vector<uint8_t>& data) {
    uint64_t sum = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] >= 128) {
            sum += data[i];
        }
    }
    return sum;
}

// 2. Branchless version: straight-line arithmetic / no jumps
uint64_t sum_branchless(const std::vector<uint8_t>& data) {
    uint64_t sum = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        // Boolean evaluates to 1 or 0; multiplies without branching
        sum += (data[i] >= 128) * data[i];
    }
    return sum;
}

int main(int argc, char* argv[]) {
    std::string mode = (argc > 1) ? argv[1] : "branch_random";
    constexpr size_t N = 32 * 1024 * 1024;

    std::vector<uint8_t> data(N);

    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> dist(0, 255);
    for (size_t i = 0; i < N; ++i) {
        data[i] = static_cast<uint8_t>(dist(rng));
    }

    if (mode.find("sorted") != std::string::npos) {
        std::sort(data.begin(), data.end());
    }

    uint64_t total = 0;
    constexpr int ITERATIONS = 10;

    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < ITERATIONS; ++iter) {
        if (mode.find("branchless") != std::string::npos) {
            total += sum_branchless(data);
        } else {
            total += sum_branching(data);
        }
        do_not_optimize(total);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    std::cout << "Mode: " << mode
              << " | Time: " << elapsed.count() << " ms"
              << " | Result: " << total << std::endl;

    return 0;
}