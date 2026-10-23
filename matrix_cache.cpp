/*
use this to compile - clang++ -std=c++20 -O2 -g matrix_cache.cpp -o matrix_cache
-02 is the optimization level, -g is for debugging symbols, and -std=c++20 specifies the C++ standard version to use.


to track row major output - 
# 1. Profile Cache-Friendly (Row Major)
valgrind --tool=cachegrind --cache-sim=yes \
  --cachegrind-out-file=cache.row.out ./matrix_cache r

# 2. Profile Cache-Hostile (Column Major)
valgrind --tool=cachegrind --cache-sim=yes \
  --cachegrind-out-file=cache.col.out ./matrix_cache c

Ir / I1mr / LLimr: Instruction reads, $L1i$ misses, and Last-Level instruction misses.   
Dr / Dw: Data cache memory reads and writes.   
D1mr / D1mw: $L1d$ Data cache read and write misses.   
LLd misses: Last-Level Data cache misses (accesses that missed all cache levels and had to go to main memory).



Simulate a machine with a restricted 32 KB, 8-way, 64-byte line $L1d$ and 1 MB unified $LLC$ using --D1 and --LL (or --L2): 
valgrind --tool=cachegrind \
  --D1=32768,8,64 \
  --LL=1048576,8,64 \
  ./matrix_cache r
*/

#include <iostream>
#include <vector>

constexpr int N = 2048;

// Read sequentially across rows (Spatial Locality)
long long row_major(const std::vector<int>& matrix) {
    long long sum = 0;
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            sum += matrix[i * N + j];
        }
    }
    return sum;
}

// Read strided down columns (8192 bytes jump per iteration -> Cache Thrashing)
long long col_major(const std::vector<int>& matrix) {
    long long sum = 0;
    for (int j = 0; j < N; ++j) {
        for (int i = 0; i < N; ++i) {
            sum += matrix[i * N + j];
        }
    }
    return sum;
}

int main(int argc, char* argv[]) {
    // 2048 x 2048 * 4 bytes = 16 MB heap allocation
    // Initialized with argc so the compiler cannot constant-fold the values at compile time
    std::vector<int> matrix(N * N, argc);

    long long result = 0;
    if (argc > 1 && argv[1][0] == 'c') {
        result = col_major(matrix);
    } else {
        result = row_major(matrix);
    }

    // Printing 'result' guarantees Clang CANNOT eliminate the traversal
    std::cout << "Sum: " << result << std::endl;
    return 0;
}