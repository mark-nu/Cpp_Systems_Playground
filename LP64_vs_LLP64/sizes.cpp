#include <iostream>
#include <cstddef>
#include <climits>

static const char *detect_data_model()
{
    const bool long_is_8 = (sizeof(long) == 8);
    const bool long_is_4 = (sizeof(long) == 4);
    const bool llong_is_8 = (sizeof(long long) == 8);
    const bool ptr_is_8 = (sizeof(void *) == 8);
    const bool ptr_is_4 = (sizeof(void *) == 4);
    const bool int_is_4 = (sizeof(int) == 4);

    // Common models:
    // LP64:   int=32, long=64, pointer=64  (Linux/macOS 64-bit)
    // LLP64:  int=32, long=32, long long=64, pointer=64 (Windows 64-bit)
    // ILP32:  int=32, long=32, pointer=32 (many 32-bit)
    if (int_is_4 && long_is_8 && ptr_is_8)
        return "LP64";
    if (int_is_4 && long_is_4 && llong_is_8 && ptr_is_8)
        return "LLP64";
    if (int_is_4 && long_is_4 && ptr_is_4)
        return "ILP32";
    return "Unknown (exotic/embedded?)";
}

int main()
{
    std::cout
        << "sizeof(int)      = " << sizeof(int) << "\n"
        << "sizeof(long)     = " << sizeof(long) << "\n"
        << "sizeof(long long)= " << sizeof(long long) << "\n"
        << "sizeof(void*)    = " << sizeof(void *) << "\n"
        << "sizeof(size_t)   = " << sizeof(std::size_t) << "\n"
        << "Model guess      = " << detect_data_model() << "\n";

    // Helpful macros you might see
#if defined(_WIN64)
    std::cout << "_WIN64 defined (Windows 64-bit)\n";
#endif
#if defined(__LP64__) || defined(_LP64)
    std::cout << "__LP64__/_LP64 defined (many LP64 toolchains)\n";
#endif
    return 0;
}
