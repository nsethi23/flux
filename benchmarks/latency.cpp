#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include "order_book.h"

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static double cycles_to_ns(uint64_t cycles) {
    // Codespaces Intel CPU ~2.8 GHz
    return (double)cycles / 2.8;
}

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

        uint64_t start = rdtsc();
        book.add_order(o);
        uint64_t end = rdtsc();

        samples.push_back(end - start);
    }

    std::sort(samples.begin(), samples.end());

    std::cout << "p50:  " << cycles_to_ns(samples[NUM_SAMPLES * 0.50]) << " ns\n";
    std::cout << "p99:  " << cycles_to_ns(samples[NUM_SAMPLES * 0.99]) << " ns\n";
    std::cout << "p999: " << cycles_to_ns(samples[NUM_SAMPLES * 0.999]) << " ns\n";

    return 0;
}