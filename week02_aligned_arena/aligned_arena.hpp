#ifndef ALIGNED_ARENA_HPP
#define ALIGNED_ARENA_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

class AlignedArena {
public:
    static constexpr std::size_t CapacityBytes = 64 * 1024 * 1024;
    static constexpr std::size_t MinAllocationSize = 16;
    static constexpr std::size_t MaxAllocationSize = 4096;
    static constexpr std::size_t SizeClassCount =
        MaxAllocationSize / MinAllocationSize;
    static constexpr std::size_t MaxAlignment = 4096;

    AlignedArena();
    ~AlignedArena();

    AlignedArena(const AlignedArena&) = delete;
    AlignedArena& operator=(const AlignedArena&) = delete;
    AlignedArena(AlignedArena&&) = delete;
    AlignedArena& operator=(AlignedArena&&) = delete;

    // Requests from 1 to 4096 bytes are rounded up to a 16-byte size class.
    // Alignment must be a power of two no greater than MaxAlignment.
    void* allocate(std::size_t bytes,
                   std::size_t alignment = alignof(std::max_align_t));
    void deallocate(void* pointer, std::size_t bytes);

    std::size_t bytesUsed() const;

private:
    using FreeListHeads = std::array<void*, SizeClassCount>;

    static FreeListHeads& threadFreeLists(std::uint64_t arenaId);
    static std::size_t classIndex(std::size_t bytes);

    std::byte* buffer_;
    std::size_t offset_{0};
    std::uint64_t arenaId_;
    mutable std::mutex bumpMutex_;

    static std::atomic<std::uint64_t> nextArenaId_;
};

#endif
