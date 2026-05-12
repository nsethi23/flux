#include "flux/order_book.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string_view>

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kIterations = 100'000;
constexpr flux::Price kBasePrice = 10'000;

struct BenchmarkResult {
    std::string_view name;
    double ns_per_op{};
};

void print_result(BenchmarkResult result) {
    std::cout << std::left << std::setw(28) << result.name << std::right << std::fixed
              << std::setprecision(2) << result.ns_per_op << " ns/op\n";
}

template <typename Func>
BenchmarkResult run_benchmark(std::string_view name, int operations, Func func) {
    const auto start = Clock::now();
    func();
    const auto end = Clock::now();

    const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return {.name = name, .ns_per_op = static_cast<double>(elapsed_ns) / operations};
}

BenchmarkResult bench_add_resting_limit_orders() {
    flux::OrderBook book;

    return run_benchmark("add resting limit", kIterations, [&book] {
        for (int i = 0; i < kIterations; ++i) {
            book.add_limit_order(
                {
                    .id = static_cast<flux::OrderId>(i + 1),
                    .side = flux::Side::Buy,
                    .price = kBasePrice - (i % 100),
                    .quantity = 100,
                }
            );
        }
    });
}

BenchmarkResult bench_cancel_orders() {
    flux::OrderBook book;
    for (int i = 0; i < kIterations; ++i) {
        book.add_limit_order(
            {
                .id = static_cast<flux::OrderId>(i + 1),
                .side = flux::Side::Buy,
                .price = kBasePrice,
                .quantity = 100,
            }
        );
    }

    return run_benchmark("cancel resting order", kIterations, [&book] {
        for (int i = 0; i < kIterations; ++i) {
            book.cancel_order(static_cast<flux::OrderId>(i + 1));
        }
    });
}

BenchmarkResult bench_aggressive_limit_matches() {
    flux::OrderBook book;
    for (int i = 0; i < kIterations; ++i) {
        book.add_limit_order(
            {
                .id = static_cast<flux::OrderId>(i + 1),
                .side = flux::Side::Sell,
                .price = kBasePrice,
                .quantity = 100,
            }
        );
    }

    return run_benchmark("limit match", kIterations, [&book] {
        for (int i = 0; i < kIterations; ++i) {
            book.add_limit_order(
                {
                    .id = static_cast<flux::OrderId>(kIterations + i + 1),
                    .side = flux::Side::Buy,
                    .price = kBasePrice,
                    .quantity = 100,
                }
            );
        }
    });
}

BenchmarkResult bench_market_order_matches() {
    flux::OrderBook book;
    for (int i = 0; i < kIterations; ++i) {
        book.add_limit_order(
            {
                .id = static_cast<flux::OrderId>(i + 1),
                .side = flux::Side::Sell,
                .price = kBasePrice,
                .quantity = 100,
            }
        );
    }

    return run_benchmark("market match", kIterations, [&book] {
        for (int i = 0; i < kIterations; ++i) {
            book.add_market_order(
                {
                    .id = static_cast<flux::OrderId>(kIterations + i + 1),
                    .side = flux::Side::Buy,
                    .price = 0,
                    .quantity = 100,
                }
            );
        }
    });
}

}  // namespace

int main() {
    std::cout << "Flux order book benchmarks\n";
    std::cout << "Iterations: " << kIterations << "\n\n";

    print_result(bench_add_resting_limit_orders());
    print_result(bench_cancel_orders());
    print_result(bench_aggressive_limit_matches());
    print_result(bench_market_order_matches());

    return 0;
}
