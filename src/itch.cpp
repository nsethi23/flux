#include "flux/itch.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace flux::itch {
namespace {

constexpr std::size_t kHeaderOffset = 1;
constexpr std::size_t kAddOrderSize = 36;
constexpr std::size_t kAddOrderWithMpidSize = 40;
constexpr std::size_t kOrderExecutedSize = 31;
constexpr std::size_t kOrderExecutedWithPriceSize = 36;
constexpr std::size_t kOrderCancelSize = 23;
constexpr std::size_t kOrderDeleteSize = 19;
constexpr std::size_t kOrderReplaceSize = 35;
constexpr std::size_t kStockDirectorySize = 39;

std::uint8_t byte_at(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint8_t>(bytes[offset]);
}

std::uint16_t read_u16(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(byte_at(bytes, offset)) << 8U) |
                                      static_cast<std::uint16_t>(byte_at(bytes, offset + 1)));
}

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(byte_at(bytes, offset)) << 24U) |
           (static_cast<std::uint32_t>(byte_at(bytes, offset + 1)) << 16U) |
           (static_cast<std::uint32_t>(byte_at(bytes, offset + 2)) << 8U) |
           static_cast<std::uint32_t>(byte_at(bytes, offset + 3));
}

std::uint64_t read_u48(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 6; ++i) {
        value = (value << 8U) | byte_at(bytes, offset + i);
    }
    return value;
}

std::uint64_t read_u64(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        value = (value << 8U) | byte_at(bytes, offset + i);
    }
    return value;
}

template <std::size_t Size>
std::string read_padded_string(std::span<const std::byte> bytes, std::size_t offset) {
    std::array<char, Size> chars{};
    for (std::size_t i = 0; i < Size; ++i) {
        chars[i] = static_cast<char>(byte_at(bytes, offset + i));
    }

    std::string value(chars.data(), chars.size());
    while (!value.empty() && value.back() == ' ') {
        value.pop_back();
    }
    return value;
}

std::string read_stock(std::span<const std::byte> bytes, std::size_t offset) {
    return read_padded_string<8>(bytes, offset);
}

std::optional<Side> read_side(std::span<const std::byte> bytes, std::size_t offset) {
    const char value = static_cast<char>(byte_at(bytes, offset));
    if (value == 'B') {
        return Side::Buy;
    }
    if (value == 'S') {
        return Side::Sell;
    }
    return std::nullopt;
}

MessageHeader read_header(std::span<const std::byte> bytes) {
    return {
        .stock_locate = read_u16(bytes, kHeaderOffset),
        .tracking_number = read_u16(bytes, kHeaderOffset + 2),
        .timestamp = read_u48(bytes, kHeaderOffset + 4),
    };
}

ParseResult wrong_size() {
    return {.error = ParseError::WrongMessageSize};
}

}  // namespace

ParseResult parse_message(std::span<const std::byte> bytes) {
    if (bytes.empty()) {
        return wrong_size();
    }

    const char message_type = static_cast<char>(byte_at(bytes, 0));
    switch (message_type) {
        case 'A': {
            if (bytes.size() != kAddOrderSize) {
                return wrong_size();
            }

            const auto side = read_side(bytes, 19);
            if (!side.has_value()) {
                return {.error = ParseError::InvalidSide};
            }

            return {
                .message =
                    AddOrder{
                        .header = read_header(bytes),
                        .order_id = read_u64(bytes, 11),
                        .side = *side,
                        .quantity = read_u32(bytes, 20),
                        .stock = read_stock(bytes, 24),
                        .price = static_cast<Price>(read_u32(bytes, 32)),
                    },
            };
        }
        case 'F': {
            if (bytes.size() != kAddOrderWithMpidSize) {
                return wrong_size();
            }

            const auto side = read_side(bytes, 19);
            if (!side.has_value()) {
                return {.error = ParseError::InvalidSide};
            }

            return {
                .message =
                    AddOrderWithMpid{
                        .header = read_header(bytes),
                        .order_id = read_u64(bytes, 11),
                        .side = *side,
                        .quantity = read_u32(bytes, 20),
                        .stock = read_stock(bytes, 24),
                        .price = static_cast<Price>(read_u32(bytes, 32)),
                        .attribution = read_padded_string<4>(bytes, 36),
                    },
            };
        }
        case 'E': {
            if (bytes.size() != kOrderExecutedSize) {
                return wrong_size();
            }

            return {
                .message =
                    OrderExecuted{
                        .header = read_header(bytes),
                        .order_id = read_u64(bytes, 11),
                        .executed_quantity = read_u32(bytes, 19),
                        .match_number = read_u64(bytes, 23),
                    },
            };
        }
        case 'C': {
            if (bytes.size() != kOrderExecutedWithPriceSize) {
                return wrong_size();
            }

            return {
                .message =
                    OrderExecutedWithPrice{
                        .header = read_header(bytes),
                        .order_id = read_u64(bytes, 11),
                        .executed_quantity = read_u32(bytes, 19),
                        .match_number = read_u64(bytes, 23),
                        .printable = static_cast<char>(byte_at(bytes, 31)) == 'Y',
                        .execution_price = static_cast<Price>(read_u32(bytes, 32)),
                    },
            };
        }
        case 'X': {
            if (bytes.size() != kOrderCancelSize) {
                return wrong_size();
            }

            return {
                .message =
                    OrderCancel{
                        .header = read_header(bytes),
                        .order_id = read_u64(bytes, 11),
                        .canceled_quantity = read_u32(bytes, 19),
                    },
            };
        }
        case 'D': {
            if (bytes.size() != kOrderDeleteSize) {
                return wrong_size();
            }

            return {
                .message =
                    OrderDelete{
                        .header = read_header(bytes),
                        .order_id = read_u64(bytes, 11),
                    },
            };
        }
        case 'U': {
            if (bytes.size() != kOrderReplaceSize) {
                return wrong_size();
            }

            return {
                .message =
                    OrderReplace{
                        .header = read_header(bytes),
                        .original_order_id = read_u64(bytes, 11),
                        .new_order_id = read_u64(bytes, 19),
                        .quantity = read_u32(bytes, 27),
                        .price = static_cast<Price>(read_u32(bytes, 31)),
                    },
            };
        }
        case 'R': {
            if (bytes.size() != kStockDirectorySize) {
                return wrong_size();
            }

            return {
                .message =
                    StockDirectory{
                        .header = read_header(bytes),
                        .stock = read_stock(bytes, 11),
                        .market_category = static_cast<char>(byte_at(bytes, 19)),
                        .financial_status_indicator = static_cast<char>(byte_at(bytes, 20)),
                        .round_lot_size = read_u32(bytes, 21),
                        .round_lots_only = static_cast<char>(byte_at(bytes, 25)) == 'Y',
                    },
            };
        }
        default:
            return {.error = ParseError::UnknownMessageType};
    }
}

FeedStreamResult parse_feed(
    std::span<const std::byte> bytes,
    const std::function<void(const FeedMessage&)>& on_message
) {
    FeedStreamResult result;
    std::size_t offset = 0;

    while (offset < bytes.size()) {
        if (bytes.size() - offset < 2) {
            result.error = FeedError::TruncatedLength;
            result.error_offset = offset;
            return result;
        }

        const std::uint16_t message_size = read_u16(bytes, offset);
        offset += 2;

        if (bytes.size() - offset < message_size) {
            result.error = FeedError::TruncatedMessage;
            result.error_offset = offset - 2;
            return result;
        }

        const auto message_bytes = bytes.subspan(offset, message_size);
        auto parsed = parse_message(message_bytes);
        if (!parsed.message.has_value()) {
            if (parsed.error == ParseError::UnknownMessageType) {
                ++result.skipped_unknown_messages;
                offset += message_size;
                continue;
            }

            result.error = FeedError::MessageParseError;
            result.error_offset = offset;
            result.parse_error = parsed.error;
            return result;
        }

        on_message(
            {
                .offset = offset,
                .message = *parsed.message,
            }
        );
        ++result.parsed_messages;
        offset += message_size;
    }

    return result;
}

FeedParseResult parse_feed(std::span<const std::byte> bytes) {
    FeedParseResult result;

    const auto stream_result = parse_feed(
        bytes,
        [&result](const FeedMessage& message) {
            result.messages.push_back(message);
        }
    );

    result.error = stream_result.error;
    result.error_offset = stream_result.error_offset;
    result.parse_error = stream_result.parse_error;
    result.skipped_unknown_messages = stream_result.skipped_unknown_messages;
    return result;
}

}  // namespace flux::itch
