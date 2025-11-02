#include "ObjectPool.hpp"
#include <vector>

struct Probe
{
    static int live, ctors, dtors;
    int v{};
    explicit Probe(int x = 0) noexcept : v(x)
    {
        ++live;
        ++ctors;
    }
    ~Probe() noexcept
    {
        --live;
        ++dtors;
    }
};
int Probe::live = 0, Probe::ctors = 0, Probe::dtors = 0;

struct Suite
{
    static constexpr int N = 1000;
    static constexpr int M = 10;

    static void construct_destroy()
    {
        ObjectPool<Probe, N> pool;
        std::vector<Probe *> ps;
        ps.reserve(M);

        for (int i = 0; i < M; ++i)
            ps.push_back(pool.create(i));
        assert(pool.free_slots() == N - M);

        for (auto *p : ps)
            pool.destroy(p);
        assert(pool.free_slots() == N);
        assert(Probe::live == 0 && Probe::ctors == Probe::dtors);
    }

    static void exhaustion()
    {
        ObjectPool<Probe, N> pool;
        std::vector<Probe *> ps;
        ps.reserve(N);

        for (int i = 0; i < N; ++i)
        {
            auto *p = pool.create(i);
            assert(p != nullptr);
            ps.push_back(p);
        }
        assert(pool.create(123) == nullptr); // full
        for (auto *p : ps)
            pool.destroy(p);
        assert(pool.free_slots() == N);
    }

    static void recycle_lifo()
    {
        ObjectPool<Probe, N> pool;
        auto *a = pool.create(1);
        auto *b = pool.create(2);
        auto *c = pool.create(3);
        pool.destroy(b);          // free middle
        auto *d = pool.create(4); // should reuse b’s slot (LIFO of free list)
        // Optional: check addresses
        assert(reinterpret_cast<void *>(d) == reinterpret_cast<void *>(b));
        pool.destroy(a);
        pool.destroy(c);
        pool.destroy(d);
        assert(pool.free_slots() == N);
    }
};

int main()
{
    Suite::construct_destroy();
    Suite::exhaustion();
    Suite::recycle_lifo();
    return 0;
}