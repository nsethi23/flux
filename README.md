# Flux

Flux is a C++ order book and matching engine project focused on clear implementation, measurable performance, and realistic market-data parsing.

## Build

```sh
cmake -S . -B build
cmake --build build
./build/flux
```

## Test

```sh
ctest --test-dir build --output-on-failure
```

## Benchmark

Use a release build for meaningful benchmark numbers:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/flux_bench
```

## Current Structure

- `CMakeLists.txt`: top-level build configuration.
- `include/flux/`: public project headers.
- `src/`: executable and implementation files.

The build now creates:

- `flux_core`: reusable library for the matching-engine code.
- `flux`: small executable that links against `flux_core`.
- `flux_bench`: simple benchmark executable for core order-book operations.

## Design Direction

Initial goals:

- Limit and market order support.
- Price-time priority: best price first, FIFO within the same price level.
- `std::map` for sorted price levels.
- Per-price FIFO queues for resting orders.
- `std::unordered_map` for direct order lookup by order ID.
- Avoid dynamic allocation in the matching hot path.
- Parse NASDAQ ITCH binary market data.
- Benchmark core operations in nanoseconds per operation.
