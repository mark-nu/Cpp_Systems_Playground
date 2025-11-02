#include <type_traits>
#include <new>
#include <array>
#include <cstdint>
#include <cassert>
#include <limits>

template <class T, std::size_t N>
class ObjectPool
{
public:
    ObjectPool() noexcept
    {
        static_assert(N <= static_cast<std::size_t>(std::numeric_limits<int>::max()), "N too large for int-based freelist");

        for (std::size_t i = 0; i < N - 1; ++i)
        {
            _next_free[i] = static_cast<int>(i + 1);
        }
        _next_free[N - 1] = _kEmpty; // Last slot points to no next free slot
    }

    ObjectPool(const ObjectPool &) = delete;
    ObjectPool &operator=(const ObjectPool &) = delete;
    ~ObjectPool() noexcept = default;
    ObjectPool(ObjectPool &&) = delete;
    ObjectPool &operator=(ObjectPool &&) = delete;

    template <class... Args>
    T *create(Args &&...args)
    {
        int index = allocate_index();
        if (index == _kEmpty)
        {
            return nullptr; // No free slots available
        }

        void *addr = ptr_from_index(index);
        try
        {
            return ::new (addr) T(std::forward<Args>(args)...); // Placement new to construct T in the allocated slot
        }
        catch (...)
        {
            release_index(index); // If construction fails, release the index
            throw;                // Re-throw the exception
        }
    }

    void destroy(T *p) noexcept(std::is_nothrow_destructible<T>::value)
    {
        assert(p != nullptr);
        int index = index_from_ptr(p);
        assert(index >= 0 && index < static_cast<int>(N));

        p->~T();              // Call destructor
        release_index(index); // Release the slot back to the pool
    }

    constexpr std::size_t capacity() const noexcept
    {
        return N;
    }

    std::size_t free_slots() const noexcept
    {
        return _free_count;
    }

private:
    using Slot = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

    Slot _buf[N];
    std::array<int, N> _next_free{};
    int _head = 0;               // Initialize head to the first slot
    std::size_t _free_count = N; // All slots are initially free
    static constexpr int _kEmpty = -1;

    static_assert(N > 0, "Pool capacity N must be greater than 0");

    // non-template helpers
    T *ptr_from_index(int i)
    {
        // address of _buf[i], then (later) we'll use placement new
        return reinterpret_cast<T *>(reinterpret_cast<void *>(&_buf[i]));
    }

    int index_from_ptr(const T *p) const
    {
        // byte distance / slot size = index
        auto base = reinterpret_cast<const char *>(static_cast<const void *>(&_buf[0]));
        auto pc = reinterpret_cast<const char *>(static_cast<const void *>(p));

        std::ptrdiff_t bytes = pc - base;
        assert(bytes % sizeof(Slot) == 0);

        int i = static_cast<int>(bytes / sizeof(Slot));
        assert(i >= 0 && i < static_cast<int>(N));

        return i;
    }

    int allocate_index()
    {
        if (_free_count == 0)
        {
            return _kEmpty; // No free slots available
        }

        int index = _head;
        _head = _next_free[index];
        --_free_count;

        return index;
    }

    void release_index(int i)
    {
        assert(i >= 0 && i < static_cast<int>(N));
        _next_free[i] = _head; // Link the released slot to the head
        _head = i;             // Update head to the newly freed slot
        ++_free_count;         // Increase free count
    }
};