#pragma once

#include <cstdint>

enum class Side : uint8_t {
    BID = 0,
    ASK = 1
};

struct alignas(64) Order {
    uint64_t order_id;
    int64_t price;
    uint64_t quantity;
    Side side;
    uint8_t padding1[7];
    uint64_t timestamp;
    uint8_t padding2[24];
};

static_assert(sizeof(Order) == 64, "Order must be exactly 64 bytes");

