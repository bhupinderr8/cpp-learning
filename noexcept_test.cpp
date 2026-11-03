#include <iostream>
#include <vector>
#include <chrono>

struct TypeCopyFallback {
    int* data;
    TypeCopyFallback() : data(new int[1000]) {}
    ~TypeCopyFallback() { delete[] data; }

    // Copy constructor
    TypeCopyFallback(const TypeCopyFallback& other) : data(new int[1000]) {
        //std::cout << "[Fallback] Copy\n"; // Uncomment for small runs
    }

    // Move constructor WITHOUT noexcept
    TypeCopyFallback(TypeCopyFallback&& other) noexcept(false) : data(other.data) {
        other.data = nullptr;
    }
};

struct TypeNoexceptMove {
    int* data;
    TypeNoexceptMove() : data(new int[1000]) {}
    ~TypeNoexceptMove() { delete[] data; }

    // Copy constructor
    TypeNoexceptMove(const TypeNoexceptMove& other) : data(new int[1000]) {}

    // Move constructor WITH noexcept
    TypeNoexceptMove(TypeNoexceptMove&& other) noexcept : data(other.data) {
        other.data = nullptr;
    }
};

int main() {
    constexpr int N = 100'000;

    // Test 1: Fallback to copies during vector growth
    auto t1 = std::chrono::high_resolution_clock::now();
    {
        std::vector<TypeCopyFallback> v1;
        for (int i = 0; i < N; ++i) {
            v1.emplace_back();
        }
    }
    auto t2 = std::chrono::high_resolution_clock::now();

    // Test 2: True zero-copy pointer transfers during vector growth
    auto t3 = std::chrono::high_resolution_clock::now();
    {
        std::vector<TypeNoexceptMove> v2;
        for (int i = 0; i < N; ++i) {
            v2.emplace_back();
        }
    }
    auto t4 = std::chrono::high_resolution_clock::now();

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
    auto d2 = std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count();

    std::cout << "Without noexcept (copies): " << d1 << " ms\n";
    std::cout << "With noexcept    (moves):  " << d2 << " ms\n";

    return 0;
}