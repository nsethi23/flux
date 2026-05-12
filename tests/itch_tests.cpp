#include "flux/itch.hpp"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
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
    push_u16(bytes, 12);
    push_u16(bytes, 34);
    push_u48(bytes, 0x010203040506ULL);
}

void push_stock(std::vector<std::byte>& bytes, std::string_view stock) {
    for (std::size_t i = 0; i < 8; ++i) {
        const char value = i < stock.size() ? stock[i] : ' ';
        bytes.push_back(static_cast<std::byte>(value));
    }
}

void append_framed_message(std::vector<std::byte>& feed, const std::vector<std::byte>& message) {
    push_u16(feed, static_cast<std::uint16_t>(message.size()));
    feed.insert(feed.end(), message.begin(), message.end());
}

std::vector<std::byte> add_order_message(
    std::uint64_t order_id,
    char side,
    std::uint32_t quantity,
    std::string_view stock,
    std::uint32_t price
) {
    std::vector<std::byte> bytes;
    bytes.push_back(static_cast<std::byte>('A'));
    push_header(bytes);
    push_u64(bytes, order_id);
    bytes.push_back(static_cast<std::byte>(side));
    push_u32(bytes, quantity);
    push_stock(bytes, stock);
    push_u32(bytes, price);
    return bytes;
}

std::vector<std::byte> cancel_message(std::uint64_t order_id, std::uint32_t quantity) {
    std::vector<std::byte> bytes;
    bytes.push_back(static_cast<std::byte>('X'));
    push_header(bytes);
    push_u64(bytes, order_id);
    push_u32(bytes, quantity);
    return bytes;
}

void test_parse_add_order() {
    std::vector<std::byte> bytes;
    bytes.push_back(static_cast<std::byte>('A'));
    push_header(bytes);
    push_u64(bytes, 123);
    bytes.push_back(static_cast<std::byte>('B'));
    push_u32(bytes, 100);
    push_stock(bytes, "AAPL");
    push_u32(bytes, 18'7500);

    const auto result = flux::itch::parse_message(bytes);

    expect(result.message.has_value(), "add order parses successfully");
    expect(!result.error.has_value(), "add order has no parse error");

    const auto* message = std::get_if<flux::itch::AddOrder>(&*result.message);
    expect(message != nullptr, "parsed message is AddOrder");
    expect(message->header.stock_locate == 12, "add order stock locate parsed");
    expect(message->header.tracking_number == 34, "add order tracking number parsed");
    expect(message->header.timestamp == 0x010203040506ULL, "add order timestamp parsed");
    expect(message->order_id == 123, "add order id parsed");
    expect(message->side == flux::Side::Buy, "add order side parsed");
    expect(message->quantity == 100, "add order quantity parsed");
    expect(message->stock == "AAPL", "add order stock parsed and trimmed");
    expect(message->price == 18'7500, "add order price parsed");
}

void test_parse_order_executed() {
    std::vector<std::byte> bytes;
    bytes.push_back(static_cast<std::byte>('E'));
    push_header(bytes);
    push_u64(bytes, 123);
    push_u32(bytes, 40);
    push_u64(bytes, 999);

    const auto result = flux::itch::parse_message(bytes);
    const auto* message = std::get_if<flux::itch::OrderExecuted>(&*result.message);

    expect(message != nullptr, "parsed message is OrderExecuted");
    expect(message->order_id == 123, "executed order id parsed");
    expect(message->executed_quantity == 40, "executed quantity parsed");
    expect(message->match_number == 999, "match number parsed");
}

void test_parse_order_cancel() {
    std::vector<std::byte> bytes;
    bytes.push_back(static_cast<std::byte>('X'));
    push_header(bytes);
    push_u64(bytes, 123);
    push_u32(bytes, 25);

    const auto result = flux::itch::parse_message(bytes);
    const auto* message = std::get_if<flux::itch::OrderCancel>(&*result.message);

    expect(message != nullptr, "parsed message is OrderCancel");
    expect(message->order_id == 123, "cancel order id parsed");
    expect(message->canceled_quantity == 25, "canceled quantity parsed");
}

void test_parse_order_delete() {
    std::vector<std::byte> bytes;
    bytes.push_back(static_cast<std::byte>('D'));
    push_header(bytes);
    push_u64(bytes, 123);

    const auto result = flux::itch::parse_message(bytes);
    const auto* message = std::get_if<flux::itch::OrderDelete>(&*result.message);

    expect(message != nullptr, "parsed message is OrderDelete");
    expect(message->order_id == 123, "delete order id parsed");
}

void test_reject_unknown_message_type() {
    const std::vector<std::byte> bytes{static_cast<std::byte>('?')};

    const auto result = flux::itch::parse_message(bytes);

    expect(!result.message.has_value(), "unknown message has no parsed value");
    expect(result.error == flux::itch::ParseError::UnknownMessageType, "unknown message error returned");
}

void test_reject_wrong_size() {
    const std::vector<std::byte> bytes{static_cast<std::byte>('A')};

    const auto result = flux::itch::parse_message(bytes);

    expect(!result.message.has_value(), "wrong size message has no parsed value");
    expect(result.error == flux::itch::ParseError::WrongMessageSize, "wrong size error returned");
}

void test_reject_invalid_side() {
    std::vector<std::byte> bytes;
    bytes.push_back(static_cast<std::byte>('A'));
    push_header(bytes);
    push_u64(bytes, 123);
    bytes.push_back(static_cast<std::byte>('Z'));
    push_u32(bytes, 100);
    push_stock(bytes, "AAPL");
    push_u32(bytes, 18'7500);

    const auto result = flux::itch::parse_message(bytes);

    expect(!result.message.has_value(), "invalid side message has no parsed value");
    expect(result.error == flux::itch::ParseError::InvalidSide, "invalid side error returned");
}

void test_parse_feed_with_multiple_messages() {
    std::vector<std::byte> feed;
    append_framed_message(feed, add_order_message(123, 'B', 100, "AAPL", 18'7500));
    append_framed_message(feed, cancel_message(123, 25));

    const auto result = flux::itch::parse_feed(feed);

    expect(!result.error.has_value(), "valid feed has no feed error");
    expect(result.messages.size() == 2, "valid feed parses two messages");
    expect(
        std::holds_alternative<flux::itch::AddOrder>(result.messages[0].message),
        "first feed message is add order"
    );
    expect(
        std::holds_alternative<flux::itch::OrderCancel>(result.messages[1].message),
        "second feed message is cancel"
    );
    expect(result.messages[0].offset == 2, "first feed message offset is after length prefix");
}

void test_parse_feed_rejects_truncated_length() {
    const std::vector<std::byte> feed{static_cast<std::byte>(0x00)};

    const auto result = flux::itch::parse_feed(feed);

    expect(result.error == flux::itch::FeedError::TruncatedLength, "truncated length error returned");
    expect(result.error_offset == 0, "truncated length offset reported");
    expect(result.messages.empty(), "truncated length produces no messages");
}

void test_parse_feed_rejects_truncated_message() {
    std::vector<std::byte> feed;
    push_u16(feed, 36);
    feed.push_back(static_cast<std::byte>('A'));

    const auto result = flux::itch::parse_feed(feed);

    expect(result.error == flux::itch::FeedError::TruncatedMessage, "truncated message error returned");
    expect(result.error_offset == 0, "truncated message offset points to frame start");
    expect(result.messages.empty(), "truncated message produces no messages");
}

void test_parse_feed_reports_message_parse_error() {
    std::vector<std::byte> feed;
    append_framed_message(feed, add_order_message(123, 'Z', 100, "AAPL", 18'7500));

    const auto result = flux::itch::parse_feed(feed);

    expect(result.error == flux::itch::FeedError::MessageParseError, "message parse error returned");
    expect(result.parse_error == flux::itch::ParseError::InvalidSide, "underlying parse error preserved");
    expect(result.error_offset == 2, "message parse error offset points to payload");
}

}  // namespace

int main() {
    test_parse_add_order();
    test_parse_order_executed();
    test_parse_order_cancel();
    test_parse_order_delete();
    test_reject_unknown_message_type();
    test_reject_wrong_size();
    test_reject_invalid_side();
    test_parse_feed_with_multiple_messages();
    test_parse_feed_rejects_truncated_length();
    test_parse_feed_rejects_truncated_message();
    test_parse_feed_reports_message_parse_error();

    if (failures != 0) {
        std::cerr << failures << " test failure(s)\n";
        return EXIT_FAILURE;
    }

    std::cout << "All ITCH tests passed\n";
    return EXIT_SUCCESS;
}
