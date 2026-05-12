#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

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

struct AddOrderWithMpid {
    MessageHeader header;
    OrderId order_id{};
    Side side{};
    Quantity quantity{};
    std::string stock;
    Price price{};
    std::string attribution;
};

struct OrderExecuted {
    MessageHeader header;
    OrderId order_id{};
    Quantity executed_quantity{};
    MatchNumber match_number{};
};

struct OrderExecutedWithPrice {
    MessageHeader header;
    OrderId order_id{};
    Quantity executed_quantity{};
    MatchNumber match_number{};
    bool printable{};
    Price execution_price{};
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

struct OrderReplace {
    MessageHeader header;
    OrderId original_order_id{};
    OrderId new_order_id{};
    Quantity quantity{};
    Price price{};
};

struct StockDirectory {
    MessageHeader header;
    std::string stock;
    char market_category{};
    char financial_status_indicator{};
    std::uint32_t round_lot_size{};
    bool round_lots_only{};
};

using Message = std::variant<
    AddOrder,
    AddOrderWithMpid,
    OrderExecuted,
    OrderExecutedWithPrice,
    OrderCancel,
    OrderDelete,
    OrderReplace,
    StockDirectory>;

enum class ParseError {
    UnknownMessageType,
    WrongMessageSize,
    InvalidSide,
};

enum class FeedError {
    TruncatedLength,
    TruncatedMessage,
    MessageParseError,
};

struct ParseResult {
    std::optional<Message> message;
    std::optional<ParseError> error;
};

struct FeedMessage {
    std::size_t offset{};
    Message message;
};

struct FeedParseResult {
    std::vector<FeedMessage> messages;
    std::optional<FeedError> error;
    std::size_t error_offset{};
    std::optional<ParseError> parse_error;
};

ParseResult parse_message(std::span<const std::byte> bytes);
FeedParseResult parse_feed(std::span<const std::byte> bytes);

}  // namespace flux::itch
