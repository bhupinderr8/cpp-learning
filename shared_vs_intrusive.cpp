#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <utility>

// ============================================================================
// 1. Generic Intrusive Pointer Template
// ============================================================================

template <typename T>
class IntrusivePtr {
public:
    using element_type = T;

    constexpr IntrusivePtr() noexcept : ptr_(nullptr) {}
    constexpr IntrusivePtr(std::nullptr_t) noexcept : ptr_(nullptr) {}

    explicit IntrusivePtr(T* ptr) noexcept : ptr_(ptr) {
        if (ptr_) {
            ptr_->add_ref();
        }
    }

    ~IntrusivePtr() noexcept {
        if (ptr_) {
            ptr_->release();
        }
    }

    // Copy semantics: increment reference count
    IntrusivePtr(const IntrusivePtr& other) noexcept : ptr_(other.ptr_) {
        if (ptr_) {
            ptr_->add_ref();
        }
    }

    IntrusivePtr& operator=(const IntrusivePtr& other) noexcept {
        if (this != &other && ptr_ != other.ptr_) {
            if (other.ptr_) {
                other.ptr_->add_ref();
            }
            if (ptr_) {
                ptr_->release();
            }
            ptr_ = other.ptr_;
        }
        return *this;
    }

    // Move semantics: transfer pointer without ref-count churn
    IntrusivePtr(IntrusivePtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    IntrusivePtr& operator=(IntrusivePtr&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                ptr_->release();
            }
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    // Modifiers & Observers
    void reset(T* ptr = nullptr) noexcept {
        if (ptr != ptr_) {
            if (ptr) {
                ptr->add_ref();
            }
            if (ptr_) {
                ptr_->release();
            }
            ptr_ = ptr;
        }
    }

    void swap(IntrusivePtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
    }

    T* get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    bool operator==(const IntrusivePtr& other) const noexcept { return ptr_ == other.ptr_; }
    bool operator!=(const IntrusivePtr& other) const noexcept { return ptr_ != other.ptr_; }

private:
    T* ptr_{nullptr};
};

// ============================================================================
// 2. Target Structs (Atomic vs Non-Atomic)
// ============================================================================

// Diagnostic counters to verify destruction without memory leaks
inline std::atomic<int64_t> g_shared_destructions{0};
inline std::atomic<int64_t> g_atomic_destructions{0};
inline std::atomic<int64_t> g_local_destructions{0};

// Target A: Standard library pointee
struct SharedWidget {
    uint64_t payload{42};
    ~SharedWidget() { g_shared_destructions.fetch_add(1, std::memory_order_relaxed); }
};

// Target B: Multi-threaded intrusive pointee (uses atomic bus locks)
struct AtomicWidget {
    mutable std::atomic<uint32_t> ref_count{0};
    uint64_t payload{42};

    ~AtomicWidget() { g_atomic_destructions.fetch_add(1, std::memory_order_relaxed); }

    void add_ref() const noexcept {
        ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    void release() const noexcept {
        if (ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }

    uint32_t use_count() const noexcept {
        return ref_count.load(std::memory_order_relaxed);
    }
};

// Target C: Thread-per-core / Reactor intrusive pointee (pure scalar, no atomic prefix)
struct LocalWidget {
    mutable uint32_t ref_count{0};
    uint64_t payload{42};

    ~LocalWidget() { g_local_destructions.fetch_add(1, std::memory_order_relaxed); }

    void add_ref() const noexcept {
        ++ref_count;
    }

    void release() const noexcept {
        if (--ref_count == 0) {
            delete this;
        }
    }

    uint32_t use_count() const noexcept {
        return ref_count;
    }
};

// ============================================================================
// 3. Isolated Functions for Assembly Analysis
// ============================================================================

// Compile with -S -masm=intel to observe:
// 1. Double indirection: rdi -> control block -> lock inc/xadd
__attribute__((noinline))
void copy_std_shared(const std::shared_ptr<SharedWidget>& src,
                     std::shared_ptr<SharedWidget>& dst) noexcept {
    dst = src;
}

// 2. Single indirection: rdi -> lock inc directly at payload offset
__attribute__((noinline))
void copy_intrusive_atomic(const IntrusivePtr<AtomicWidget>& src,
                           IntrusivePtr<AtomicWidget>& dst) noexcept {
    dst = src;
}

// 3. Single indirection, bare scalar instruction: inc dword ptr [rdi] (no lock)
__attribute__((noinline))
void copy_intrusive_local(const IntrusivePtr<LocalWidget>& src,
                          IntrusivePtr<LocalWidget>& dst) noexcept {
    dst = src;
}

// ============================================================================
// 4. Verification Suite (Unit Tests)
// ============================================================================

void run_unit_tests() {
    std::cout << "[1/2] Running Unit Tests...\n";

    // Test 1: Basic Lifecycle & Scope Destruction
    {
        g_atomic_destructions.store(0);
        g_local_destructions.store(0);

        {
            auto p1 = IntrusivePtr<AtomicWidget>(new AtomicWidget());
            assert(p1->use_count() == 1);
            {
                auto p2 = p1;
                assert(p1->use_count() == 2);
                assert(p2->use_count() == 2);
            }
            assert(p1->use_count() == 1);
        }
        assert(g_atomic_destructions.load() == 1);

        {
            auto l1 = IntrusivePtr<LocalWidget>(new LocalWidget());
            assert(l1->use_count() == 1);
            {
                auto l2 = l1;
                assert(l1->use_count() == 2);
                assert(l2->use_count() == 2);
            }
            assert(l1->use_count() == 1);
        }
        assert(g_local_destructions.load() == 1);
    }

    // Test 2: Move Semantics & Source Nulling
    {
        auto p1 = IntrusivePtr<AtomicWidget>(new AtomicWidget());
        AtomicWidget* raw = p1.get();

        IntrusivePtr<AtomicWidget> p2(std::move(p1));
        assert(p1.get() == nullptr);
        assert(p2.get() == raw);
        assert(p2->use_count() == 1);

        IntrusivePtr<AtomicWidget> p3;
        p3 = std::move(p2);
        assert(p2.get() == nullptr);
        assert(p3.get() == raw);
        assert(p3->use_count() == 1);
    }

    // Test 3: Self-Assignment Safety
    {
        auto p1 = IntrusivePtr<AtomicWidget>(new AtomicWidget());
        auto& self = p1;
        p1 = self;
        assert(p1.get() != nullptr);
        assert(p1->use_count() == 1);
    }

    // Test 4: Reset & Swap
    {
        auto a = IntrusivePtr<AtomicWidget>(new AtomicWidget());
        auto b = IntrusivePtr<AtomicWidget>(new AtomicWidget());
        AtomicWidget* raw_a = a.get();
        AtomicWidget* raw_b = b.get();

        a.swap(b);
        assert(a.get() == raw_b);
        assert(b.get() == raw_a);

        a.reset();
        assert(a.get() == nullptr);
        assert(!a);
    }

    std::cout << "      All Unit Tests Passed Successfully.\n";
}

// ============================================================================
// 5. Performance Comparison Micro-Benchmark
// ============================================================================

void run_micro_benchmarks() {
    std::cout << "[2/2] Running Micro-Benchmarks (100,000,000 Genuine Copies each)...\n";
    constexpr uint64_t kIterations = 100'000'000;

    // Benchmark A: std::shared_ptr
    {
        auto src = std::make_shared<SharedWidget>();
        auto start = std::chrono::steady_clock::now();
        for (uint64_t i = 0; i < kIterations; ++i) {
            std::shared_ptr<SharedWidget> dst(src);
            asm volatile("" : : "r"(dst.get()) : "memory");
        }
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        std::cout << "      std::shared_ptr (Heap Control Block + Atomic) : "
                  << std::fixed << std::setprecision(2) << elapsed.count() << " ms "
                  << "(" << (elapsed.count() * 1e6 / kIterations) << " ns/op)\n";
    }

    // Benchmark B: IntrusivePtr with Atomic Counter
    {
        IntrusivePtr<AtomicWidget> src(new AtomicWidget());
        auto start = std::chrono::steady_clock::now();
        for (uint64_t i = 0; i < kIterations; ++i) {
            IntrusivePtr<AtomicWidget> dst(src);
            asm volatile("" : : "r"(dst.get()) : "memory");
        }
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        std::cout << "      IntrusivePtr (In-Object Atomic Counter)       : "
                  << std::fixed << std::setprecision(2) << elapsed.count() << " ms "
                  << "(" << (elapsed.count() * 1e6 / kIterations) << " ns/op)\n";
    }

    // Benchmark C: IntrusivePtr with Local Non-Atomic Counter
    {
        IntrusivePtr<LocalWidget> src(new LocalWidget());
        auto start = std::chrono::steady_clock::now();
        for (uint64_t i = 0; i < kIterations; ++i) {
            IntrusivePtr<LocalWidget> dst(src);
            asm volatile("" : : "r"(dst.get()) : "memory");
        }
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        std::cout << "      IntrusivePtr (Reactor Scalar In-Object Counter): "
                  << std::fixed << std::setprecision(2) << elapsed.count() << " ms "
                  << "(" << (elapsed.count() * 1e6 / kIterations) << " ns/op)\n";
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    run_unit_tests();
    run_micro_benchmarks();
    return 0;
}