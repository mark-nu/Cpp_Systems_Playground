#include <vector>
#include <iostream>
#include <chrono>
#include <cstddef>
#include <algorithm>
#include <numeric>

static constexpr std::size_t N = 20000000; // tune if RAM is tight
static constexpr int REPS = 5;             // timing repetitions (median)
static constexpr float dt = 0.005f;

struct ParticleAoS
{
    float x, y, z;    // Position
    float vx, vy, vz; // Velocity
};
struct ParticleSoA
{
    std::vector<float> x, y, z;
    std::vector<float> vx, vy, vz;
};

inline void init_data(std::vector<ParticleAoS> &aos, ParticleSoA &soa)
{
    aos.resize(N);
    soa.x.resize(N);
    soa.y.resize(N);
    soa.z.resize(N);
    soa.vx.resize(N);
    soa.vy.resize(N);
    soa.vz.resize(N);
    for (std::size_t i = 0; i < N; ++i)
    {
        float p = static_cast<float>(i);
        float v = p * 0.1f;
        aos[i] = {p, p, p, v, v, v};
        soa.x[i] = p;
        soa.y[i] = p;
        soa.z[i] = p;
        soa.vx[i] = v;
        soa.vy[i] = v;
        soa.vz[i] = v;
    }
}

template <typename F>
inline long long median_time_ms(F &&f)
{
    std::vector<long long> times;
    times.reserve(REPS);
    // one warm-up (not timed)
    f();
    for (int r = 0; r < REPS; ++r)
    {
        auto t0 = std::chrono::steady_clock::now();
        f();
        auto t1 = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
    }
    std::nth_element(times.begin(), times.begin() + times.size() / 2, times.end());
    return times[times.size() / 2];
}

inline double checksum_xyz(const std::vector<ParticleAoS> &aos)
{
    double s = 0.0;
    for (std::size_t i = 0; i < aos.size(); ++i)
        s += aos[i].x + aos[i].y + aos[i].z;
    return s;
}
inline double checksum_xyz(const ParticleSoA &soa)
{
    double sx = 0.0, sy = 0.0, sz = 0.0;
    for (std::size_t i = 0; i < soa.x.size(); ++i)
    {
        sx += soa.x[i];
        sy += soa.y[i];
        sz += soa.z[i];
    }
    return sx + sy + sz;
}
inline double checksum_x(const std::vector<ParticleAoS> &aos)
{
    double s = 0.0;
    for (std::size_t i = 0; i < aos.size(); ++i)
        s += aos[i].x;
    return s;
}
inline double checksum_x(const ParticleSoA &soa)
{
    double s = 0.0;
    for (std::size_t i = 0; i < soa.x.size(); ++i)
        s += soa.x[i];
    return s;
}

int main()
{
    // ===== Case 1: Single-axis update (x only) =====
    {
        std::vector<ParticleAoS> aos;
        ParticleSoA soa;
        init_data(aos, soa);

        auto aos_ms = median_time_ms([&]()
                                     {
            for (std::size_t i = 0; i < N; ++i) {
                aos[i].x += aos[i].vx * dt;
            } });
        auto soa_ms = median_time_ms([&]()
                                     {
            for (std::size_t i = 0; i < N; ++i) {
                soa.x[i] += soa.vx[i] * dt;
            } });
        // checksums outside the timed region
        double cs_aos = checksum_x(aos);
        double cs_soa = checksum_x(soa);

        std::cout << "[Case 1] Single-axis update (x only): "
                  << "AoS=" << aos_ms << " ms, "
                  << "SoA=" << soa_ms << " ms, "
                  << "checksum(AoS)=" << cs_aos << ", "
                  << "checksum(SoA)=" << cs_soa << "\n";
    }

    // ===== Case 2: Field-wise loops (x pass, then y pass, then z pass) =====
    {
        std::vector<ParticleAoS> aos;
        ParticleSoA soa;
        init_data(aos, soa);

        auto aos_ms = median_time_ms([&]()
                                     {
            for (std::size_t i = 0; i < N; ++i) aos[i].x += aos[i].vx * dt;
            for (std::size_t i = 0; i < N; ++i) aos[i].y += aos[i].vy * dt;
            for (std::size_t i = 0; i < N; ++i) aos[i].z += aos[i].vz * dt; });
        auto soa_ms = median_time_ms([&]()
                                     {
            for (std::size_t i = 0; i < N; ++i) soa.x[i] += soa.vx[i] * dt;
            for (std::size_t i = 0; i < N; ++i) soa.y[i] += soa.vy[i] * dt;
            for (std::size_t i = 0; i < N; ++i) soa.z[i] += soa.vz[i] * dt; });

        double cs_aos = checksum_xyz(aos);
        double cs_soa = checksum_xyz(soa);

        std::cout << "[Case 2] Field-wise loops (x pass, y pass, z pass): "
                  << "AoS=" << aos_ms << " ms, "
                  << "SoA=" << soa_ms << " ms, "
                  << "checksum(AoS)=" << cs_aos << ", "
                  << "checksum(SoA)=" << cs_soa << "\n";
    }

    // ===== Case 3: Read-only sweep of x (no writes) =====
    {
        std::vector<ParticleAoS> aos;
        ParticleSoA soa;
        init_data(aos, soa);

        volatile double sink = 0.0; // ensure the sum is used
        auto aos_ms = median_time_ms([&]()
                                     {
            double s = 0.0;
            for (std::size_t i = 0; i < N; ++i) s += aos[i].x;
            sink = s; });
        auto soa_ms = median_time_ms([&]()
                                     {
            double s = 0.0;
            for (std::size_t i = 0; i < N; ++i) s += soa.x[i];
            sink = s; });

        // compute and print final sums outside timed region
        double sum_aos = checksum_x(aos);
        double sum_soa = checksum_x(soa);

        std::cout << "[Case 3] Read-only sweep of x: "
                  << "AoS=" << aos_ms << " ms, "
                  << "SoA=" << soa_ms << " ms, "
                  << "sum(AoS)=" << sum_aos << ", "
                  << "sum(SoA)=" << sum_soa << "\n";
    }

    // ===== Case 4: Double precision (update all axes in one loop) =====
    {
        struct ParticleD
        {
            double x, y, z, vx, vy, vz;
        };
        struct ParticleSoAD
        {
            std::vector<double> x, y, z, vx, vy, vz;
        };

        std::vector<ParticleD> aos;
        ParticleSoAD soa;
        aos.resize(N);
        soa.x.resize(N);
        soa.y.resize(N);
        soa.z.resize(N);
        soa.vx.resize(N);
        soa.vy.resize(N);
        soa.vz.resize(N);

        for (std::size_t i = 0; i < N; ++i)
        {
            double p = static_cast<double>(i);
            double v = p * 0.1;
            aos[i] = {p, p, p, v, v, v};
            soa.x[i] = p;
            soa.y[i] = p;
            soa.z[i] = p;
            soa.vx[i] = v;
            soa.vy[i] = v;
            soa.vz[i] = v;
        }
        const double dtd = 0.005;

        auto aos_ms = median_time_ms([&]()
                                     {
            for (std::size_t i = 0; i < N; ++i) {
                aos[i].x += aos[i].vx * dtd;
                aos[i].y += aos[i].vy * dtd;
                aos[i].z += aos[i].vz * dtd;
            } });
        auto soa_ms = median_time_ms([&]()
                                     {
            for (std::size_t i = 0; i < N; ++i) {
                soa.x[i] += soa.vx[i] * dtd;
                soa.y[i] += soa.vy[i] * dtd;
                soa.z[i] += soa.vz[i] * dtd;
            } });

        // checksum for doubles (xyz)
        auto checksum_xyz_aos_d = [&]()
        {
            long double s = 0.0L;
            for (std::size_t i = 0; i < N; ++i)
                s += aos[i].x + aos[i].y + aos[i].z;
            return static_cast<double>(s);
        };
        auto checksum_xyz_soa_d = [&]()
        {
            long double sx = 0.0L, sy = 0.0L, sz = 0.0L;
            for (std::size_t i = 0; i < N; ++i)
            {
                sx += soa.x[i];
                sy += soa.y[i];
                sz += soa.z[i];
            }
            return static_cast<double>(sx + sy + sz);
        };

        double cs_aos = checksum_xyz_aos_d();
        double cs_soa = checksum_xyz_soa_d();

        std::cout << "[Case 4] Double precision (update x,y,z): "
                  << "AoS=" << aos_ms << " ms, "
                  << "SoA=" << soa_ms << " ms, "
                  << "checksum(AoS)=" << cs_aos << ", "
                  << "checksum(SoA)=" << cs_soa << "\n";
    }

    return 0;
}
