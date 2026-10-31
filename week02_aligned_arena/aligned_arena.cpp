#include "aligned_arena.hpp"

#include <limits>
#include <memory>
#include <new>
#include <stdexcept>

using aligned_arena_detail::FreeNode;

std::atomic<std::uint64_t> AlignedArena::nextArenaId_{1};

AlignedArena::AlignedArena()
    : buffer_(static_cast<std::byte*>(
          ::operator new(CapacityBytes, std::align_val_t{CacheLineAlignment}))),
      arenaId_(nextArenaId_.fetch_add(1, std::memory_order_relaxed)) {}

AlignedArena::~AlignedArena() {
    ::operator delete(buffer_, std::align_val_t{CacheLineAlignment});
}

std::size_t AlignedArena::classIndex(std::size_t bytes) {
    if (bytes == 0 || bytes > MaxAllocationSize) {
        throw std::invalid_argument(
            "AlignedArena size class requires a size from 1 to 4096 bytes");
    }

    const std::size_t belowMinimumMask =
        std::size_t{0} - static_cast<std::size_t>(bytes < MinAllocationSize);
    const std::size_t normalizedBytes =
        (bytes & ~belowMinimumMask) | (MinAllocationSize & belowMinimumMask);
    return static_cast<std::size_t>(
               std::countr_zero(std::bit_ceil(normalizedBytes))) -
           static_cast<std::size_t>(std::countr_zero(MinAllocationSize));
}

void* AlignedArena::allocate(std::size_t bytes, std::size_t alignment) {
    if (bytes == 0) {
        throw std::invalid_argument("AlignedArena allocation size must be nonzero");
    }
    if (alignment == 0 || (alignment & (alignment - 1)) != 0 ||
        alignment > MaxAlignment) {
        throw std::invalid_argument(
            "AlignedArena alignment must be a power of two no greater than 4096");
    }
    const bool isSmallAllocation = bytes <= MaxAllocationSize;
    if (isSmallAllocation && alignment < MinAllocationSize) {
        alignment = MinAllocationSize;
    }

    std::size_t index = 0;
    std::size_t allocationSize = bytes;
    if (isSmallAllocation) {
        index = classIndex(bytes);
        allocationSize = MinAllocationSize << index;

        if (aligned_arena_detail::t_arena_id != arenaId_) {
            aligned_arena_detail::t_heads.fill(nullptr);
            aligned_arena_detail::t_arena_id = arenaId_;
        }

        FreeNode* previousNode = nullptr;
        FreeNode* node = aligned_arena_detail::t_heads[index];
        while (node != nullptr) {
            if (reinterpret_cast<std::uintptr_t>(node) % alignment == 0) {
                if (previousNode == nullptr) {
                    aligned_arena_detail::t_heads[index] = node->next;
                } else {
                    previousNode->next = node->next;
                }
                std::destroy_at(node);
                return node;
            }
            previousNode = node;
            node = node->next;
        }
    }

    const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(buffer_);
    std::size_t currentOffset = offset_.load(std::memory_order_relaxed);
    while (true) {
        if (currentOffset > CapacityBytes) {
            throw std::bad_alloc();
        }
        const std::uintptr_t current = base + currentOffset;
        if (current >
            std::numeric_limits<std::uintptr_t>::max() - (alignment - 1)) {
            throw std::bad_alloc();
        }

        const std::uintptr_t alignedPointer =
            (current + (alignment - 1)) &
            ~(static_cast<std::uintptr_t>(alignment) - 1);
        const std::size_t alignedOffset =
            static_cast<std::size_t>(alignedPointer - base);
        if (alignedOffset > CapacityBytes ||
            allocationSize > CapacityBytes - alignedOffset) {
            throw std::bad_alloc();
        }

        const std::size_t nextOffset = alignedOffset + allocationSize;
        if (offset_.compare_exchange_weak(currentOffset, nextOffset,
                                          std::memory_order_relaxed,
                                          std::memory_order_relaxed)) {
            return reinterpret_cast<void*>(alignedPointer);
        }
    }
}

void AlignedArena::deallocate(void* pointer, std::size_t bytes) {
    if (pointer == nullptr || bytes > MaxAllocationSize) {
        return;
    }

    const std::size_t index = classIndex(bytes);
    if (aligned_arena_detail::t_arena_id != arenaId_) {
        aligned_arena_detail::t_heads.fill(nullptr);
        aligned_arena_detail::t_arena_id = arenaId_;
    }

    auto* node = static_cast<FreeNode*>(pointer);
    std::construct_at(
        node, FreeNode{aligned_arena_detail::t_heads[index]});
    aligned_arena_detail::t_heads[index] = node;
}

std::size_t AlignedArena::bytesUsed() const {
    return offset_.load(std::memory_order_relaxed);
}
