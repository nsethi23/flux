#pragma once

#include <cstdint>
#include <deque>
#include "order.h"

struct PriceLevel {
    int64_t price;
    uint64_t total_quantity;
    std::deque<Order> orders;

    PriceLevel(int64_t p) : price(p), total_quantity(0) {} 
};

