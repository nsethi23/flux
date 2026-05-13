# Flux

Flux is a C++20 order book and matching-engine project built for quant developer / HFT internship recruiting. It focuses on correct market microstructure behavior, realistic binary market-data parsing, tests, and nanosecond-level benchmarks.

## What It Demonstrates

- Price-time priority matching.
- Limit and market orders.
- Good-till-cancel, IOC, and FOK behavior.
- Cancel, reduce, and replace order operations.
- O(1) cancel after order-ID lookup using stored FIFO iterators.
- Trade and top-of-book listener callbacks.
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

Benchmarks currently cover core order-book operations plus ITCH parsing and replay. Results are reported as min/p50/p99/p99.9/max nanoseconds per operation over repeated samples.

Sample release run on this development machine:

```text
add resting limit           min    69.73 p50    74.91 p99    97.36 p99.9    97.36 max    97.36 ns/op
cancel resting order        min    26.35 p50    27.21 p99    28.52 p99.9    28.52 max    28.52 ns/op
limit match                 min    52.91 p50    54.22 p99    75.28 p99.9    75.28 max    75.28 ns/op
market match                min    54.01 p50    55.78 p99    97.28 p99.9    97.28 max    97.28 ns/op
mixed order flow            min    57.96 p50    60.77 p99    63.90 p99.9    63.90 max    63.90 ns/op
ITCH parse message          min     6.77 p50     6.93 p99    10.52 p99.9    10.52 max    10.52 ns/op
ITCH parse feed             min     8.99 p50     9.70 p99    16.77 p99.9    16.77 max    16.77 ns/op
ITCH replay feed            min    91.39 p50    95.76 p99   114.33 p99.9   114.33 max   114.33 ns/op
```

## Replay ITCH

`flux_replay` reads a binary ITCH file containing 2-byte big-endian length-prefixed messages, parses it, and replays supported messages into the matching engine. Unsupported message types are skipped and counted.

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
