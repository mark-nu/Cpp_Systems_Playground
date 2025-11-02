# C++ Systems Playground

Small, focused C++ experiments about performance, memory layout, concurrency, and ABI/portability. Each folder is a self‑contained demo with a short `main()` you can compile and run directly.

> Measure on your own machine: build with optimizations (`-O3`/`/O2`) and keep runs consistent (same flags, CPU governor, background load).

---

## Repository Structure

```
.
├── AoS_vs_SoA_Traversal/
├── False_Sharing_Demo/
├── LP64_vs_LLP64/
├── Lock_Free_Ring_Buffer/
├── Pool_Allocator_w_Placement_New/
├── Vector_Reallocation_&_noexcept_Move/
├── scripts/
│   ├── build_one.sh
│   └── build_one.ps1
├── CMakeLists.txt
└── .gitignore
```

### What’s in each folder

- **`AoS_vs_SoA_Traversal/`** — Array‑of‑Structs vs Struct‑of‑Arrays traversal and update patterns. Demonstrates cache‑friendly passes, read‑only sweeps, and checksum guards.
- **`False_Sharing_Demo/`** — Two threads contending on the same cache line vs. padded/aligned fields. Shows the impact of false sharing on throughput/latency.
- **`LP64_vs_LLP64/`** — Prints `sizeof` of fundamental types and classifies the host data model (LP64/LLP64/ILP32). Handy for quick portability checks.
- **`Lock_Free_Ring_Buffer/`** — Single‑producer/single‑consumer ring buffer with power‑of‑two capacity and acquire/release memory orderings. Includes a tiny throughput benchmark.
- **`Pool_Allocator_w_Placement_New/`** — Minimal fixed‑slot pool using placement new / explicit destruction. Probe program verifies capacity limits and LIFO reuse of freed slots.
- **`Vector_Reallocation_&_noexcept_Move/`** — Explores how `std::vector` growth interacts with move vs copy and `noexcept` on move constructors.

---

## Build Options

You can build **per‑folder** (simplest), or from the **repo root** using the top‑level `CMakeLists.txt` you’ve added.

### A) Build from repo root

The top‑level `CMakeLists.txt` adds each demo as a subdirectory and includes **toggles** so users don’t have to build everything.

```bash
# Configure (Release default unless overridden)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build all enabled demos
cmake --build build --parallel
```

Executables typically land under their demo’s build dir (generator‑dependent). Common layouts:

```
build/AoS_vs_SoA_Traversal/aos_soa
build/False_Sharing_Demo/false_sharing
build/LP64_vs_LLP64/sizes
build/Lock_Free_Ring_Buffer/spsc
build/Pool_Allocator_w_Placement_New/pool_probe
build/Vector_Reallocation_&_noexcept_Move/vector_moves
```

**Toggle specific demos at configure time:**

```bash
# Only build SPSC
cmake -S . -B build -DBUILD_SPSC=ON \
                 -DBUILD_AOS_SOA=OFF -DBUILD_FALSE_SHARING=OFF \
                 -DBUILD_SIZES=OFF -DBUILD_POOL_PROBE=OFF -DBUILD_VECTOR_MOVES=OFF
cmake --build build --parallel
./build/Lock_Free_Ring_Buffer/spsc 65536 20000000
```

> The top‑level file keeps the same warnings/standard as the per‑folder `CMakeLists.txt`. Use `-DCMAKE_BUILD_TYPE=Debug` for debug builds.

### B) Per‑folder builds

Each folder contains its own `CMakeLists.txt` so users can build only what they want **without** the top‑level.

```bash
# From repo root
cd AoS_vs_SoA_Traversal
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/aos_soa

cd ../Lock_Free_Ring_Buffer
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/spsc 65536 20000000
```

Windows (PowerShell):

```powershell
# From folder you want to build (e.g., Lock_Free_Ring_Buffer)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
.\build\spsc.exe 65536 20000000
```

> **Notes**
>
> - All demos are C++17. Build files set sensible warnings and enable `-march=native` in Release on non‑MSVC by default (toggle with `-DENABLE_MARCH_NATIVE=OFF`).
> - Concurrency demos link with Threads (CMake’s `Threads::Threads`).

---

## Helper Scripts

Convenience wrappers to build a **single** demo without remembering CMake incantations.

### macOS/Linux

```bash
# Build from repo root
scripts/build_one.sh spsc
# Build + run with args
scripts/build_one.sh spsc --run 65536 20000000
# Debug build
scripts/build_one.sh false_sharing --debug
# Clean rebuild
scripts/build_one.sh aos_soa --clean
```

### Windows (PowerShell)

```powershell
# Build + run
scripts\build_one.ps1 -target spsc -run -- 65536 20000000

# Debug build
scripts\build_one.ps1 -target false_sharing -debug

# Clean rebuild
scripts\build_one.ps1 -target aos_soa -clean
```

Supported targets: `aos_soa`, `false_sharing`, `sizes`, `spsc`, `pool_probe`, `vector_moves`.

---

## Tips for Reproducible Numbers

- **Release builds** only (`-DCMAKE_BUILD_TYPE=Release` → `-O3` or `/O2`).
- **CPU scaling**: keep clock speeds steady; consider disabling turbo when comparing runs.
- **NUMA/affinity** (for multi‑socket servers): pin threads for the concurrency demos.
- **Warm up** once; take the **median** of several runs.
- **Input sizes**: large `N` stress memory bandwidth; tune for your machine.

---

## License

This project is licensed under the **MIT License**.  
Copyright (c) 2025 Mark Nudelman  
See the [LICENSE](./LICENSE) file for the full text.

> Add an SPDX tag at the top of source files if you like:
>
> ```cpp
> // SPDX-License-Identifier: MIT
> ```

---

## Contributing

PRs welcome for additional small, well‑commented demos. Please keep each demo self‑contained and portable (C++17), and include a brief comment header describing purpose, usage, and tunables.
