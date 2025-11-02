#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

static constexpr std::size_t M = 300000; // keep memory reasonable

struct TNoNoexcept
{
    static std::size_t copies, moves;
    std::string s;
    TNoNoexcept() : s(100, 'x') {}
    TNoNoexcept(const TNoNoexcept &o) : s(o.s) { ++copies; }
    TNoNoexcept(TNoNoexcept &&o) /* not noexcept */ : s(std::move(o.s)) { ++moves; }
};

struct TNoexcept
{
    static std::size_t copies, moves;
    std::string s;
    TNoexcept() : s(100, 'x') {}
    TNoexcept(const TNoexcept &o) : s(o.s) { ++copies; }
    TNoexcept(TNoexcept &&o) noexcept : s(std::move(o.s)) { ++moves; }
};

// out-of-class defs (required in C++)
std::size_t TNoNoexcept::copies = 0;
std::size_t TNoNoexcept::moves = 0;
std::size_t TNoexcept::copies = 0;
std::size_t TNoexcept::moves = 0;

template <class T>
void run_emplace(const char *label)
{
    T::copies = 0;
    T::moves = 0;
    std::vector<T> v;
    std::size_t reallocs = 0;
    for (std::size_t i = 0; i < M; ++i)
    {
        std::size_t cap_before = v.capacity();
        v.emplace_back(); // construct in-place; any copies/moves here come from reallocation only
        if (v.capacity() != cap_before)
            ++reallocs;
    }
    std::cout << label << " (emplace): "
              << "size=" << v.size()
              << " reallocs=" << reallocs
              << " copies=" << T::copies
              << " moves=" << T::moves << "\n";
}

template <class T>
void run_push(const char *label)
{
    T::copies = 0;
    T::moves = 0;
    std::vector<T> v;
    std::size_t reallocs = 0;
    for (std::size_t i = 0; i < M; ++i)
    {
        std::size_t cap_before = v.capacity();
        v.push_back(T()); // adds ~M extra moves from inserting temporaries
        if (v.capacity() != cap_before)
            ++reallocs;
    }
    std::cout << label << " (push_back): "
              << "size=" << v.size()
              << " reallocs=" << reallocs
              << " copies=" << T::copies
              << " moves=" << T::moves
              << "  (insertion moves ~= " << M << ")\n";
}

int main()
{
    run_emplace<TNoNoexcept>("TNoNoexcept"); // expect: copies > 0, moves ~ 0 (copies used on reallocation)
    run_emplace<TNoexcept>("TNoexcept");     // expect: copies == 0, moves > 0 (moves used on reallocation)
    std::cout << "----\n";
    run_push<TNoNoexcept>("TNoNoexcept"); // expect: copies > 0, moves ≈ M + (realloc moves)
    run_push<TNoexcept>("TNoexcept");     // expect: copies == 0, moves > M
}