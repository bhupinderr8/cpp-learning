#include <iostream>
#include <cstddef>  // for offsetof
#include <cstdint>  // for fixed-width int types

// Unoptimized struct layout
struct BadLayout {
    char a;        // 1 byte
    int64_t b;     // 8 bytes
    int32_t c;     // 4 bytes
};

// Optimized struct layout (Reordered by descending field size)
struct GoodLayout {
    int64_t b;     // 8 bytes
    int32_t c;     // 4 bytes
    char a;        // 1 bytes
};

int main() {
    std::cout << "=== BadLayout ===" << std::endl;
    std::cout << "Size:        " << sizeof(BadLayout) << " bytes" << std::endl;
    std::cout << "Alignment:   " << alignof(BadLayout) << " bytes" << std::endl;
    std::cout << "Offset of a: " << offsetof(BadLayout, a) << " byte" << std::endl;
    std::cout << "Offset of b: " << offsetof(BadLayout, b) << " bytes" << std::endl;
    std::cout << "Offset of c: " << offsetof(BadLayout, c) << " bytes" << std::endl;

    std::cout << "\n=== GoodLayout ===" << std::endl;
    std::cout << "Size:        " << sizeof(GoodLayout) << " bytes" << std::endl;
    std::cout << "Alignment:   " << alignof(GoodLayout) << " bytes" << std::endl;
    std::cout << "Offset of b: " << offsetof(GoodLayout, b) << " byte" << std::endl;
    std::cout << "Offset of c: " << offsetof(GoodLayout, c) << " bytes" << std::endl;
    std::cout << "Offset of a: " << offsetof(GoodLayout, a) << " bytes" << std::endl;

    return 0;
}