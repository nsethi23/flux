# Performance Notes

Flux currently prioritizes correctness and explainability, then measures the cost of that implementation.

## Current Complexity

| Operation | Current complexity |
| --- | --- |
| Best bid / best ask | O(1) |
| Add resting order at existing price | O(1) average ID insert, plus container cost |
| Add resting order at new price | O(log P), where P is number of price levels |
| Match next resting order | O(1) at the best price level |
| Cancel by order ID | O(1) average ID lookup plus O(1) FIFO erase |
| Reduce quantity by order ID | O(1) average ID lookup |
| Replace by order ID | O(1) average ID lookup plus new-price insertion cost |
| Parse one supported ITCH message | O(1), fixed-size field decoding |
| Parse length-prefixed ITCH feed | O(N), where N is number of bytes |

## Dynamic Allocation Today

The current implementation still allocates dynamically through:

- `std::map` price-level nodes.
- `std::unordered_map` order entries.
- `std::list` FIFO nodes.
- `std::vector<Trade>` result storage.
- `std::string` symbols and ITCH stock strings.

That means the project does not yet satisfy "no dynamic allocation in the hot path." It has the correct behavior and measurement scaffolding needed before replacing these pieces.

`std::list` is used because stable iterators make O(1) cancel straightforward. This is not cache-optimal: each node can live in a different heap allocation. The intended production direction is a custom pool-backed intrusive queue that preserves O(1) cancel while improving locality and allocation behavior.

## Likely Optimization Path

1. Reserve expected order-map capacity up front.
2. Replace `std::vector<Trade>` results with caller-owned fixed-capacity buffers.
3. Replace `std::list` with a pool-backed intrusive queue.
4. Replace `std::map` with a price ladder or sparse indexed structure where the tick range is known.
5. Add allocation counters to benchmarks.
6. Add realistic mixed workloads instead of isolated single-operation loops.
7. Replace `std::string` in ITCH message structs with fixed-width symbol/MPID fields.

## Benchmarking

Use release builds:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/flux_bench
```

For local CPU-specific benchmark builds:

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DFLUX_ENABLE_NATIVE_ARCH=ON
cmake --build build-release
```

For sanitizer builds:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DFLUX_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

The current benchmark reports min, p50, p99, p99.9, and max nanoseconds per operation over repeated samples. Treat the numbers as comparative signals, not final production claims.

Current benchmark categories:

- isolated order-book operations
- deterministic mixed order flow
- single-message ITCH parsing
- length-prefixed feed parsing
- ITCH replay into `MatchingEngine`
