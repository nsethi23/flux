#include "flux/itch.hpp"
#include "flux/itch_replay.hpp"
#include "flux/matching_engine.hpp"
#include "flux/order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kIterations = 10'000;
constexpr int kSamples = 9;
constexpr flux::Price kBasePrice = 10'000;

struct BenchmarkResult {
    std::string_view name;
    double min_ns_per_op{};
    double p50_ns_per_op{};
    double p99_ns_per_op{};
    double p999_ns_per_op{};
    double max_ns_per_op{};
};

void print_result(BenchmarkResult result) {
    std::cout << std::left << std::setw(28) << result.name << std::right << std::fixed
              << std::setprecision(2) << "min " << std::setw(8) << result.min_ns_per_op << " p50 "
              << std::setw(8) << result.p50_ns_per_op << " p99 " << std::setw(8)
              << result.p99_ns_per_op << " p99.9 " << std::setw(8) << result.p999_ns_per_op
              << " max " << std::setw(8) << result.max_ns_per_op << " ns/op\n";
}

double percentile(const std::vector<double>& sorted_samples, double percentile_value) {
    const auto rank = std::ceil((percentile_value / 100.0) * static_cast<double>(sorted_samples.size()));
    const auto index = std::min<std::size_t>(
        static_cast<std::size_t>(rank) - 1,
        sorted_samples.size() - 1
    );
    return sorted_samples[index];
}

template <typename SetupFunc, typename Func>
BenchmarkResult run_benchmark(std::string_view name, int operations, SetupFunc setup, Func func) {
    std::vector<double> samples;
    samples.reserve(kSamples);

    setup();
    func();

    for (int sample = 0; sample < kSamples; ++sample) {
        setup();
        const auto start = Clock::now();
        func();
        const auto end = Clock::now();

        const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        samples.push_back(static_cast<double>(elapsed_ns) / operations);
    }

    std::ranges::sort(samples);

    return {
        .name = name,
        .min_ns_per_op = samples.front(),
        .p50_ns_per_op = percentile(samples, 50.0),
        .p99_ns_per_op = percentile(samples, 99.0),
        .p999_ns_per_op = percentile(samples, 99.9),
        .max_ns_per_op = samples.back(),
    };
}

template <typename Func>
BenchmarkResult run_benchmark(std::string_view name, int operations, Func func) {
    return run_benchmark(name, operations, [] {}, func);
}

void push_u16(std::vector<std::byte>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
}

void push_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<std::byte>((value >> 24U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
}

void push_u48(std::vector<std::byte>& bytes, std::uint64_t value) {
    for (int shift = 40; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

void push_u64(std::vector<std::byte>& bytes, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

void push_header(std::vector<std::byte>& bytes) {
    push_u16(bytes, 1);
    push_u16(bytes, 1);
    push_u48(bytes, 1);
}

void push_stock(std::vector<std::byte>& bytes, std::string_view stock) {
    for (std::size_t i = 0; i < 8; ++i) {
        const char value = i < stock.size() ? stock[i] : ' ';
        bytes.push_back(static_cast<std::byte>(value));
    }
}

std::vector<std::byte> make_add_order_message(flux::OrderId order_id) {
    std::vector<std::byte> bytes;
    bytes.reserve(36);
    bytes.push_back(static_cast<std::byte>('A'));
    push_header(bytes);
    push_u64(bytes, order_id);
    bytes.push_back(static_cast<std::byte>('B'));
    push_u32(bytes, 100);
    push_stock(bytes, "AAPL");
    push_u32(bytes, 18'7500);
    return bytes;
}

std::vector<std::byte> make_feed(int messages) {
    std::vector<std::byte> feed;
    feed.reserve(static_cast<std::size_t>(messages) * 38);

    for (int i = 0; i < messages; ++i) {
        const auto message = make_add_order_message(static_cast<flux::OrderId>(i + 1));
        push_u16(feed, static_cast<std::uint16_t>(message.size()));
        feed.insert(feed.end(), message.begin(), message.end());
    }

    return feed;
}

BenchmarkResult bench_add_resting_limit_orders() {
    return run_benchmark("add resting limit", kIterations, [] {
        flux::OrderBook book;
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

    auto setup = [&book] {
        book = flux::OrderBook(static_cast<std::size_t>(kIterations));
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
    };

    return run_benchmark("cancel resting order", kIterations, setup, [&book] {
        for (int i = 0; i < kIterations; ++i) {
            book.cancel_order(static_cast<flux::OrderId>(i + 1));
        }
    });
}

BenchmarkResult bench_aggressive_limit_matches() {
    flux::OrderBook book;

    auto setup = [&book] {
        book = flux::OrderBook(static_cast<std::size_t>(kIterations));
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
    };

    return run_benchmark("limit match", kIterations, setup, [&book] {
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

    auto setup = [&book] {
        book = flux::OrderBook(static_cast<std::size_t>(kIterations));
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
    };

    return run_benchmark("market match", kIterations, setup, [&book] {
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

BenchmarkResult bench_mixed_order_flow() {
    return run_benchmark("mixed order flow", kIterations, [] {
        flux::OrderBook book;
        flux::OrderId next_id = 1;

        for (int i = 0; i < kIterations; ++i) {
            const int action = i % 10;

            if (action < 6) {
                book.add_limit_order(
                    {
                        .id = next_id++,
                        .side = action % 2 == 0 ? flux::Side::Buy : flux::Side::Sell,
                        .price = kBasePrice + ((i % 21) - 10),
                        .quantity = 100,
                    }
                );
            } else if (action < 8) {
                const flux::OrderId candidate = static_cast<flux::OrderId>((i / 2) + 1);
                book.cancel_order(candidate);
            } else if (action == 8) {
                book.add_market_order(
                    {
                        .id = next_id++,
                        .side = flux::Side::Buy,
                        .price = 0,
                        .quantity = 50,
                    }
                );
            } else {
                book.add_limit_order(
                    {
                        .id = next_id++,
                        .side = flux::Side::Sell,
                        .price = kBasePrice - 5,
                        .quantity = 50,
                    }
                );
            }
        }
    });
}

BenchmarkResult bench_parse_single_itch_message() {
    const auto message = make_add_order_message(1);

    return run_benchmark("ITCH parse message", kIterations, [&message] {
        for (int i = 0; i < kIterations; ++i) {
            const auto parsed = flux::itch::parse_message(message);
            if (!parsed.message.has_value()) {
                std::abort();
            }
        }
    });
}

BenchmarkResult bench_parse_itch_feed() {
    const auto feed = make_feed(kIterations);

    return run_benchmark("ITCH parse feed", kIterations, [&feed] {
        std::size_t parsed_count = 0;
        const auto parsed = flux::itch::parse_feed(
            feed,
            [&parsed_count](const flux::itch::FeedMessage& /*message*/) {
                ++parsed_count;
            }
        );
        if (parsed.error.has_value() || parsed_count != static_cast<std::size_t>(kIterations)) {
            std::abort();
        }
    });
}

BenchmarkResult bench_replay_itch_feed() {
    const auto feed = make_feed(kIterations);
    std::vector<flux::itch::FeedMessage> messages;
    messages.reserve(kIterations);
    const auto parsed = flux::itch::parse_feed(
        feed,
        [&messages](const flux::itch::FeedMessage& message) {
            messages.push_back(message);
        }
    );
    if (parsed.error.has_value()) {
        std::abort();
    }

    return run_benchmark("ITCH replay feed", kIterations, [&messages] {
        flux::MatchingEngine engine;
        flux::itch::ReplayHandler replay{engine};
        const auto summary = replay.apply_all(messages);
        if (summary.added != static_cast<std::size_t>(kIterations)) {
            std::abort();
        }
    });
}

}  // namespace

int main() {
    std::cout << "Flux order book benchmarks\n";
    std::cout << "Iterations: " << kIterations << "\n\n";
    std::cout << "Each benchmark reports " << kSamples << " measured samples after one warmup sample.\n";
    std::cout << "Setup is included where the operation requires a preloaded book.\n\n";

    print_result(bench_add_resting_limit_orders());
    print_result(bench_cancel_orders());
    print_result(bench_aggressive_limit_matches());
    print_result(bench_market_order_matches());
    print_result(bench_mixed_order_flow());
    print_result(bench_parse_single_itch_message());
    print_result(bench_parse_itch_feed());
    print_result(bench_replay_itch_feed());

    return 0;
}
