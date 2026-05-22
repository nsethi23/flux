#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <time.h>
#include "order_book.h"

#ifdef __aarch64__
// Mac ARM
static inline uint64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1'000'000'000ULL + ts.tv_nsec;
}
static double to_ns(uint64_t t) { return (double)t; }

#else
// Linux x86
static inline uint64_t now_ns() {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
static double to_ns(uint64_t cycles) { return (double)cycles / 2.8; }
#endif

int main() {
    const int NUM_SAMPLES = 1'000'000;
    std::vector<uint64_t> samples;
    samples.reserve(NUM_SAMPLES);

    OrderBook book;
    uint64_t id = 0;

    for (int i = 0; i < NUM_SAMPLES; ++i) {
        Order o;
        o.order_id = ++id;
        o.price = 100 + (i % 10);
        o.quantity = 10;
        o.side = (i % 2 == 0) ? Side::BID : Side::ASK;
        o.timestamp = 0;

        uint64_t start = now_ns();
        book.add_order(o);
        uint64_t end = now_ns();

        samples.push_back(end - start);
    }

    std::sort(samples.begin(), samples.end());

    std::cout << "p50:  " << to_ns(samples[NUM_SAMPLES * 0.50]) << " ns\n";
    std::cout << "p99:  " << to_ns(samples[NUM_SAMPLES * 0.99]) << " ns\n";
    std::cout << "p999: " << to_ns(samples[NUM_SAMPLES * 0.999]) << " ns\n";

    return 0;
}