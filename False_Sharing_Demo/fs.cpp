#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <cassert>
#include <iomanip>
#include <cstdint>

static constexpr std::uint64_t N = 100000000;

// This code demonstrates a case of false sharing in C++ using atomic counters.
struct CountersBad
{
    std::atomic<std::uint64_t> a, b;
};

void worker(std::atomic<std::uint64_t> &counter, std::uint64_t iters)
{
    for (std::uint64_t i = 0; i < iters; ++i)
    {
        counter.fetch_add(1, std::memory_order_relaxed);
    }
}

double run_bad(std::uint64_t iters)
{
    CountersBad counters;
    counters.a.store(0, std::memory_order_relaxed);
    counters.b.store(0, std::memory_order_relaxed);

    auto start = std::chrono::high_resolution_clock::now();

    std::thread t1(worker, std::ref(counters.a), iters);
    std::thread t2(worker, std::ref(counters.b), iters);

    t1.join();
    t2.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    assert(counters.a.load(std::memory_order_relaxed) == iters);
    assert(counters.b.load(std::memory_order_relaxed) == iters);

    auto pa = reinterpret_cast<std::uintptr_t>(&counters.a);
    auto pb = reinterpret_cast<std::uintptr_t>(&counters.b);
    std::cout << "Bad &a=" << pa << " &b=" << pb
              << " delta=" << (pb - pa) << " bytes\n";

    return duration.count();
}

struct alignas(64) Padded
{
    std::atomic<std::uint64_t> v;
};

static_assert(alignof(Padded) == 64, "Padded struct must be aligned to 64 bytes");

struct CountersGood
{
    Padded a, b;
};

double run_good(std::uint64_t iters)
{
    CountersGood counters;
    counters.a.v.store(0, std::memory_order_relaxed);
    counters.b.v.store(0, std::memory_order_relaxed);

    auto start = std::chrono::high_resolution_clock::now();

    std::thread t1(worker, std::ref(counters.a.v), iters);
    std::thread t2(worker, std::ref(counters.b.v), iters);

    t1.join();
    t2.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    assert(counters.a.v.load(std::memory_order_relaxed) == iters);
    assert(counters.b.v.load(std::memory_order_relaxed) == iters);

    auto pa = reinterpret_cast<std::uintptr_t>(&counters.a.v);
    auto pb = reinterpret_cast<std::uintptr_t>(&counters.b.v);
    std::cout << "Good &a=" << pa << " &b=" << pb
              << " delta=" << (pb - pa) << " bytes\n";

    return duration.count();
}

int main()
{
    std::cout << std::fixed << std::setprecision(3);

    std::uint64_t iters = N / 2; // Each thread will increment its counter N/2 times

    double duration1 = run_bad(iters);
    std::cout << "Bad duration: " << duration1 << " seconds\n";

    double duration2 = run_good(iters);
    std::cout << "Good duration: " << duration2 << " seconds\n";

    std::cout << "Speedup: " << (duration1 / duration2) << "x\n";

    return 0;
}