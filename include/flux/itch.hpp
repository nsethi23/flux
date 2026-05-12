#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>

#include "flux/order.hpp"

namespace flux::itch {

using Timestamp = std::uint64_t;
using StockLocate = std::uint16_t;
using TrackingNumber = std::uint16_t;
using MatchNumber = std::uint64_t;

struct MessageHeader {
    StockLocate stock_locate{};
    TrackingNumber tracking_number{};
    Timestamp timestamp{};
};

struct AddOrder {
    MessageHeader header;
    OrderId order_id{};
    Side side{};
    Quantity quantity{};
    std::string stock;
    Price price{};
};

struct OrderExecuted {
    MessageHeader header;
    OrderId order_id{};
    Quantity executed_quantity{};
    MatchNumber match_number{};
};

struct OrderCancel {
    MessageHeader header;
    OrderId order_id{};
    Quantity canceled_quantity{};
};

struct OrderDelete {
    MessageHeader header;
    OrderId order_id{};
};

using Message = std::variant<AddOrder, OrderExecuted, OrderCancel, OrderDelete>;

enum class ParseError {
    UnknownMessageType,
    WrongMessageSize,
    InvalidSide,
};

struct ParseResult {
    std::optional<Message> message;
    std::optional<ParseError> error;
};

ParseResult parse_message(std::span<const std::byte> bytes);

}  // namespace flux::itch
