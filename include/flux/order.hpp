#pragma once

#include <cstdint>

namespace flux {

using OrderId = std::uint64_t;
using Price = std::int64_t;
using Quantity = std::uint32_t;

enum class Side {
    Buy,
    Sell,
};

enum class TimeInForce {
    GoodTillCancel,
    ImmediateOrCancel,
    FillOrKill,
};

struct Order {
    OrderId id{};
    Side side{};
    Price price{};
    Quantity quantity{};
    TimeInForce time_in_force{TimeInForce::GoodTillCancel};
};

}  // namespace flux
