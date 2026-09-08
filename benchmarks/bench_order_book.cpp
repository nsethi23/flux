#include <benchmark/benchmark.h>
#include "order_book.h"

static void BM_AddOrder(benchmark::State& state) {
    OrderBook book;
    uint64_t id = 0;

    for (auto _ : state) {
        Order o;
        o.order_id = ++id;
        o.price = 100;
        o.quantity = 10;
        o.side = Side::BID;
        o.timestamp = 0;
        book.add_order(o);
        book.cancel_order(o.order_id, UINT64_MAX);
    }
}
BENCHMARK(BM_AddOrder);

BENCHMARK_MAIN();