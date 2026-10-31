#ifndef ALIGNED_ARENA_HPP
#define ALIGNED_ARENA_HPP

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace aligned_arena_detail {

struct FreeNode {
    FreeNode* next;
};

inline thread_local std::array<FreeNode*, 9> t_heads{nullptr};
// A thread reuses this fixed cache for one arena at a time.
inline thread_local std::uint64_t t_arena_id{0};

} // namespace aligned_arena_detail

class AlignedArena {
public:
    static constexpr std::size_t CapacityBytes = 64 * 1024 * 1024;
    static constexpr std::size_t MinAllocationSize = 16;
    static constexpr std::size_t MaxAllocationSize = 4096;
    static constexpr std::size_t SizeClassCount = 9;
    static constexpr std::size_t CacheLineAlignment = 64;
    static constexpr std::size_t MaxAlignment = 4096;

    AlignedArena();
    ~AlignedArena();

    AlignedArena(const AlignedArena&) = delete;
    AlignedArena& operator=(const AlignedArena&) = delete;
    AlignedArena(AlignedArena&&) = delete;
    AlignedArena& operator=(AlignedArena&&) = delete;

    // Requests up to 4096 bytes are rounded up to a power-of-two size class.
    // Alignment must be a power of two no greater than MaxAlignment.
    void* allocate(std::size_t bytes,
                   std::size_t alignment = CacheLineAlignment);
    void deallocate(void* pointer, std::size_t bytes);

    std::size_t bytesUsed() const;

private:
    static std::size_t classIndex(std::size_t bytes);

    std::byte* buffer_;
    std::atomic<std::size_t> offset_{0};
    std::uint64_t arenaId_;

    static std::atomic<std::uint64_t> nextArenaId_;
};

#endif
