#include "flux/itch.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace flux::itch {
namespace {

constexpr std::size_t kHeaderOffset = 1;
constexpr std::size_t kAddOrderSize = 36;
constexpr std::size_t kOrderExecutedSize = 31;
constexpr std::size_t kOrderCancelSize = 23;
constexpr std::size_t kOrderDeleteSize = 19;

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

std::string read_stock(std::span<const std::byte> bytes, std::size_t offset) {
    std::array<char, 8> chars{};
    for (std::size_t i = 0; i < chars.size(); ++i) {
        chars[i] = static_cast<char>(byte_at(bytes, offset + i));
    }

    std::string stock(chars.data(), chars.size());
    while (!stock.empty() && stock.back() == ' ') {
        stock.pop_back();
    }
    return stock;
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
        default:
            return {.error = ParseError::UnknownMessageType};
    }
}

}  // namespace flux::itch
