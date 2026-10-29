#include <iostream>
#include <vector>
#include <string>
#include <memory_resource>
#include <cassert>
#include <cstddef>

// 1. A custom memory_resource tracking exactly how many bytes are served
class TrackingResource : public std::pmr::memory_resource {
public:
    explicit TrackingResource(const char* name, std::pmr::memory_resource* upstream = std::pmr::get_default_resource())
        : name_(name), upstream_(upstream) {}

    std::size_t bytes_allocated() const { return bytes_allocated_; }
    std::size_t allocation_count() const { return alloc_count_; }

protected:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override {
        bytes_allocated_ += bytes;
        alloc_count_++;
        std::cout << "[" << name_ << "] Allocating " << bytes << " bytes (total: " 
                  << bytes_allocated_ << "B, count: " << alloc_count_ << ")\n";
        return upstream_->allocate(bytes, alignment);
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override {
        upstream_->deallocate(p, bytes, alignment);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }

private:
    const char* name_;
    std::pmr::memory_resource* upstream_;
    std::size_t bytes_allocated_{0};
    std::size_t alloc_count_{0};
};

int main() {
    std::cout << "=== Test 1: Nested Container Propagation ===\n";
    TrackingResource arena_res("ArenaTrack");

    {
        // Vector initialized with our custom resource
        std::pmr::vector<std::pmr::string> vec(&arena_res);

        std::cout << "-> Pushing small string (SSO, no dynamic allocation for string chars):\n";
        vec.emplace_back("short");

        std::cout << "-> Pushing large string (>15 chars, exceeds SSO):\n";
        // CRITICAL CHECK: std::pmr::vector automatically propagates its polymorphic_allocator
        // down to std::pmr::string through std::uses_allocator construction.
        vec.emplace_back("A very long string exceeding the Small String Optimization threshold");

        // The vector itself allocated buffer space from arena_res
        // The long string allocated its characters ALSO from arena_res!
        assert(vec.get_allocator().resource() == &arena_res);
        assert(vec[1].get_allocator().resource() == &arena_res);
    }

    std::cout << "\n=== Test 2: Move Invariant Across Distinct Resources ===\n";
    TrackingResource pool_a("PoolA");
    TrackingResource pool_b("PoolB");

    {
        std::pmr::vector<std::pmr::string> vec_a(&pool_a);
        vec_a.emplace_back("Another string that definitely exceeds SSO storage limit");

        std::size_t pool_b_initial = pool_b.bytes_allocated();

        // Target vector bound to PoolB
        std::pmr::vector<std::pmr::string> vec_b(&pool_b);

        std::cout << "-> Moving vec_a into vec_b with DIFFERENT allocators:\n";
        // When allocators mismatch, move assignment CANNOT simply steal the root pointer!
        // It must allocate new buffers in PoolB and copy/move elements individually.
        vec_b = std::move(vec_a);

        std::cout << "PoolB bytes after move: " << pool_b.bytes_allocated() << "\n";
        assert(pool_b.bytes_allocated() > pool_b_initial); // PoolB had to allocate
        assert(vec_b[0].get_allocator().resource() == &pool_b); // Propagated to inner string
    }

    std::cout << "\n[PASS] All allocator propagation invariants verified!\n";
    return 0;
}