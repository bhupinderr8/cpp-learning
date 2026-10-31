#include "aligned_arena.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <unordered_map>

namespace {

struct FreeNode {
    FreeNode* next;
};

std::size_t rounded_size(std::size_t bytes) {
    if (bytes == 0 || bytes > AlignedArena::MaxAllocationSize) {
        throw std::invalid_argument("AlignedArena allocation size must be from 1 to 4096 bytes");
    }

    return std::max(AlignedArena::MinAllocationSize,
                    (bytes + AlignedArena::MinAllocationSize - 1) &
                        ~(AlignedArena::MinAllocationSize - 1));
}

} // namespace

std::atomic<std::uint64_t> AlignedArena::nextArenaId_{1};

AlignedArena::AlignedArena()
    : buffer_(static_cast<std::byte*>(
          ::operator new(CapacityBytes, std::align_val_t{MaxAlignment}))),
      arenaId_(nextArenaId_.fetch_add(1, std::memory_order_relaxed)) {}

AlignedArena::~AlignedArena() {
    ::operator delete(buffer_, std::align_val_t{MaxAlignment});
}

AlignedArena::FreeListHeads& AlignedArena::threadFreeLists(
    std::uint64_t arenaId) {
    thread_local std::unordered_map<std::uint64_t, FreeListHeads> allFreeLists;
    return allFreeLists.try_emplace(arenaId, FreeListHeads{}).first->second;
}

std::size_t AlignedArena::classIndex(std::size_t bytes) {
    return rounded_size(bytes) / MinAllocationSize - 1;
}

void* AlignedArena::allocate(std::size_t bytes, std::size_t alignment) {
    const std::size_t allocation_size = rounded_size(bytes);
    if (alignment == 0 || (alignment & (alignment - 1)) != 0 ||
        alignment > MaxAlignment) {
        throw std::invalid_argument(
            "AlignedArena alignment must be a power of two no greater than 4096");
    }
    alignment = std::max(alignment, MinAllocationSize);

    const std::size_t index = classIndex(allocation_size);
    FreeListHeads& heads = threadFreeLists(arenaId_);
    FreeNode* previousNode = nullptr;
    FreeNode* node = static_cast<FreeNode*>(heads[index]);
    while (node != nullptr) {
        if (reinterpret_cast<std::uintptr_t>(node) % alignment == 0) {
            if (previousNode == nullptr) {
                heads[index] = node->next;
            } else {
                previousNode->next = node->next;
            }
            std::destroy_at(node);
            return node;
        }
        previousNode = node;
        node = node->next;
    }

    std::lock_guard<std::mutex> lock(bumpMutex_);
    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(buffer_);
    const std::uintptr_t current = base + offset_;
    if (current > std::numeric_limits<std::uintptr_t>::max() - (alignment - 1)) {
        throw std::bad_alloc();
    }

    const std::uintptr_t aligned_ptr =
        (current + (alignment - 1)) & ~(static_cast<std::uintptr_t>(alignment) - 1);
    const std::size_t alignedOffset = static_cast<std::size_t>(aligned_ptr - base);
    if (alignedOffset > CapacityBytes ||
        allocation_size > CapacityBytes - alignedOffset) {
        throw std::bad_alloc();
    }

    offset_ = alignedOffset + allocation_size;
    return reinterpret_cast<void*>(aligned_ptr);
}

void AlignedArena::deallocate(void* pointer, std::size_t bytes) {
    if (pointer == nullptr) {
        return;
    }

    const std::size_t index = classIndex(bytes);
    FreeListHeads& heads = threadFreeLists(arenaId_);
    auto* node = static_cast<FreeNode*>(pointer);
    std::construct_at(node, FreeNode{static_cast<FreeNode*>(heads[index])});
    heads[index] = node;
}

std::size_t AlignedArena::bytesUsed() const {
    std::lock_guard<std::mutex> lock(bumpMutex_);
    return offset_;
}
