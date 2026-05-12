# Flux

Flux is a C++20 order book and matching-engine project built for quant developer / HFT internship recruiting. It focuses on correct market microstructure behavior, realistic binary market-data parsing, tests, and nanosecond-level benchmarks.

## What It Demonstrates

- Price-time priority matching.
- Limit and market orders.
- Cancel, reduce, and replace order operations.
- O(1) cancel after order-ID lookup using stored FIFO iterators.
- Multi-symbol matching engine wrapper.
- NASDAQ ITCH-style binary parser and replay path.
- CMake, CTest, CI, formatting config, docs, and benchmarks.

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

The test suite covers order-book behavior, ITCH parsing, and ITCH replay.

## Benchmark

Use a release build for meaningful benchmark numbers:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/flux_bench
```

Benchmarks currently cover core order-book operations plus ITCH parsing and replay. Results are reported as min/median/max nanoseconds per operation over repeated samples.

Sample release run on this development machine:

```text
add resting limit           min    68.46 median    74.33 max    76.15 ns/op
cancel resting order        min    59.04 median    60.00 max    62.33 ns/op
limit match                 min    82.69 median    84.74 max    88.24 ns/op
market match                min    84.99 median    86.16 max   101.43 ns/op
mixed order flow            min    57.74 median    60.15 max    69.30 ns/op
ITCH parse message          min     6.18 median     6.61 max     6.75 ns/op
ITCH parse feed             min    17.08 median    20.39 max    31.38 ns/op
ITCH replay feed            min    95.46 median    97.60 max   108.62 ns/op
```

## Replay ITCH

`flux_replay` reads a binary ITCH file containing 2-byte big-endian length-prefixed messages, parses it, and replays supported messages into the matching engine:

```sh
./build/flux_replay path/to/feed.itch
```

## Supported ITCH Messages

- `A`: Add Order
- `F`: Add Order with MPID Attribution
- `E`: Order Executed
- `C`: Order Executed With Price
- `X`: Order Cancel
- `D`: Order Delete
- `U`: Order Replace
- `R`: Stock Directory

## Project Structure

- `include/flux/`: public headers.
- `src/`: library implementation and demo executable.
- `tools/`: command-line tools such as `flux_replay`.
- `tests/`: CTest test executables.
- `benchmarks/`: benchmark executable.
- `docs/`: architecture and performance notes.

Build targets:

- `flux_core`: reusable matching-engine library.
- `flux`: small demo executable.
- `flux_bench`: benchmark executable.
- `flux_replay`: binary ITCH feed replay executable.

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Performance Notes](docs/PERFORMANCE.md)

## Honest Limitations

Flux is correctness-first and measured, but not yet production HFT infrastructure. The current implementation still uses standard containers that allocate dynamically in hot paths. See [Performance Notes](docs/PERFORMANCE.md) for the planned path toward fixed-capacity buffers, object pools, and custom price-level storage.
