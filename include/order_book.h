#pragma once

#include <map>
#include <cstdint>
#include "order.h"
#include "price_level.h"

class OrderBook {
public:
    void add_order(Order order);
    void cancel_order(uint64_t order_id, uint64_t quantity);
    void execute_order(uint64_t order_id, uint64_t quantity);
    int64_t best_bid() const;
    int64_t best_ask() const;
    uint64_t best_bid_quantity() const;
    uint64_t best_ask_quantity() const;

private:
    std::map<int64_t, PriceLevel, std::greater<int64_t>> bids;
    std::map<int64_t, PriceLevel> asks;
};