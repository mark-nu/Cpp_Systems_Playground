#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <thread>
#include <chrono>
#include <iostream>
#include <cassert>
#include <memory>

// --------------------------- SPSCQueue ---------------------------
template <class T, std::size_t PowerOfTwoCapacity>
class SPSCQueue
{
public:
    SPSCQueue() noexcept : head_(0), tail_(0)
    {
        static_assert(PowerOfTwoCapacity >= 2, "Capacity must be >= 2");
        static_assert((PowerOfTwoCapacity & (PowerOfTwoCapacity - 1)) == 0,
                      "Capacity must be a power of two");
    }

    ~SPSCQueue() { clear(); }

    SPSCQueue(const SPSCQueue &) = delete;
    SPSCQueue &operator=(const SPSCQueue &) = delete;

    SPSCQueue(SPSCQueue &&) = default;
    SPSCQueue &operator=(SPSCQueue &&) = default;

    // Producer thread only
    bool try_push(const T &value)
    {
        std::size_t head = head_.load(std::memory_order_relaxed);
        std::size_t next = (head + 1) & MASK;
        if (next == tail_.load(std::memory_order_acquire))
        {
            return false; // full
        }
        ::new (elem_addr(head)) T(value); // placement-new
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Producer thread only
    bool try_push(T &&value)
    {
        std::size_t head = head_.load(std::memory_order_relaxed);
        std::size_t next = (head + 1) & MASK;
        if (next == tail_.load(std::memory_order_acquire))
        {
            return false; // full
        }
        ::new (elem_addr(head)) T(std::move(value));
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Producer thread only
    template <class... Args>
    bool try_emplace(Args &&...args)
    {
        std::size_t head = head_.load(std::memory_order_relaxed);
        std::size_t next = (head + 1) & MASK;
        if (next == tail_.load(std::memory_order_acquire))
        {
            return false; // full
        }
        ::new (elem_addr(head)) T(std::forward<Args>(args)...);
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer thread only
    bool try_pop(T &out)
    {
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
        {
            return false; // empty
        }
        T *p = elem_ptr(tail);
        out = *p; // copy or move (RVO not applicable here)
        p->~T();  // destroy once
        tail_.store((tail + 1) & MASK, std::memory_order_release);
        return true;
    }

    // Non-owning queries (racy/approximate, but often useful for debug)
    bool empty() const noexcept
    {
        return tail_.load(std::memory_order_acquire) ==
               head_.load(std::memory_order_acquire);
    }
    bool full() const noexcept
    {
        std::size_t head = head_.load(std::memory_order_acquire);
        std::size_t next = (head + 1) & MASK;
        return next == tail_.load(std::memory_order_acquire);
    }

    // Destroy any remaining elements. Call only when both threads are stopped.
    void clear() noexcept
    {
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        std::size_t head = head_.load(std::memory_order_relaxed);
        while (tail != head)
        {
            elem_ptr(tail)->~T();
            tail = (tail + 1) & MASK;
        }
        tail_.store(tail, std::memory_order_relaxed);
    }

private:
    static constexpr std::size_t CAP = PowerOfTwoCapacity;
    static constexpr std::size_t MASK = CAP - 1;
    using storage_t = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

    void *elem_addr(std::size_t i) { return &buf_[i]; }
    T *elem_ptr(std::size_t i) { return reinterpret_cast<T *>(&buf_[i]); }

    storage_t buf_[CAP];

    // Written by producer; read by consumer
    alignas(64) std::atomic<std::size_t> head_;
    // Written by consumer; read by producer
    alignas(64) std::atomic<std::size_t> tail_;
};

// --------------------------- Test / Benchmark ---------------------------
int main(int argc, char **argv)
{
    typedef std::uint64_t value_t;

    // Defaults: 1<<16 capacity, 20M items. You can tweak via args.
    const std::size_t CAP = (argc > 1) ? static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10))
                                       : (1u << 16);
    const std::size_t N = (argc > 2) ? static_cast<std::size_t>(std::strtoull(argv[2], nullptr, 10))
                                     : 20000000ull;

    // Enforce power-of-two CAP from command line in this simple demo.
    if ((CAP & (CAP - 1)) != 0 || CAP < 2)
    {
        std::cerr << "Capacity must be a power of two >= 2\n";
        return 1;
    }

    // We can’t pass CAP as a runtime value to the template—so use a switch on a few common sizes,
    // or just pick one at compile time. For flexibility, here’s a simple macro helper:
#define RUN_WITH_CAP(CAPPOW2)                                                          \
    if (CAP == (CAPPOW2))                                                              \
    {                                                                                  \
        std::unique_ptr<SPSCQueue<value_t, (CAPPOW2)>> q(                              \
            new SPSCQueue<value_t, (CAPPOW2)>());                                      \
        std::atomic<bool> go(false);                                                   \
        std::atomic<std::size_t> produced(0), consumed(0);                             \
        value_t sum = 0;                                                               \
        std::thread prod([&] {                                                       \
            while (!go.load(std::memory_order_acquire)) {}                           \
            for (std::size_t i = 0; i < N; ++i) {                                    \
                value_t v = static_cast<value_t>(i);                                 \
                while (!q->try_push(v)) {}                                           \
                produced.fetch_add(1, std::memory_order_relaxed);                    \
            } });                                                     \
        std::thread cons([&] {                                                       \
            while (!go.load(std::memory_order_acquire)) {}                           \
            value_t v;                                                               \
            for (std::size_t i = 0; i < N; ++i) {                                    \
                while (!q->try_pop(v)) {}                                            \
                sum += v;                                                            \
                consumed.fetch_add(1, std::memory_order_relaxed);                    \
            } });                                                     \
        auto t0 = std::chrono::steady_clock::now();                                    \
        go.store(true, std::memory_order_release);                                     \
        prod.join();                                                                   \
        cons.join();                                                                   \
        auto t1 = std::chrono::steady_clock::now();                                    \
        const double secs = std::chrono::duration<double>(t1 - t0).count();            \
        const unsigned long long expected = (unsigned long long)N * (N - 1ull) / 2;    \
        std::cout << "Capacity: " << (CAPPOW2) << " | N: " << N                        \
                  << " | time: " << secs << " s\n";                                    \
        std::cout << "Throughput: " << (static_cast<double>(N) / secs) << " msgs/s\n"; \
        std::cout << "Checksum OK? " << (sum == expected ? "yes" : "NO") << "\n";      \
        return 0;                                                                      \
    }

    // Support a few common power-of-two sizes conveniently
    RUN_WITH_CAP(1u << 10)
    RUN_WITH_CAP(1u << 12)
    RUN_WITH_CAP(1u << 14)
    RUN_WITH_CAP(1u << 16)
    RUN_WITH_CAP(1u << 18)
    RUN_WITH_CAP(1u << 20)

    std::cerr << "Unsupported CAP for this demo. Recompile or add to the list.\n";
    return 2;
}
