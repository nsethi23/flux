#include "flux/itch.hpp"
#include "flux/itch_replay.hpp"
#include "flux/matching_engine.hpp"

#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

namespace {

std::string_view to_string(flux::itch::ParseError error) {
    switch (error) {
        case flux::itch::ParseError::UnknownMessageType:
            return "unknown message type";
        case flux::itch::ParseError::WrongMessageSize:
            return "wrong message size";
        case flux::itch::ParseError::InvalidSide:
            return "invalid side";
    }

    return "unknown parse error";
}

std::string_view to_string(flux::itch::FeedError error) {
    switch (error) {
        case flux::itch::FeedError::TruncatedLength:
            return "truncated length";
        case flux::itch::FeedError::TruncatedMessage:
            return "truncated message";
        case flux::itch::FeedError::MessageParseError:
            return "message parse error";
    }

    return "unknown feed error";
}

std::optional<std::vector<std::byte>> read_file(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }

    std::vector<char> chars(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    std::vector<std::byte> bytes;
    bytes.reserve(chars.size());
    for (const char value : chars) {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    }

    return bytes;
}

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " <itch-file>\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 2;
    }

    const auto bytes = read_file(argv[1]);
    if (!bytes.has_value()) {
        std::cerr << "Failed to open ITCH file: " << argv[1] << '\n';
        return 1;
    }

    const auto parsed = flux::itch::parse_feed(*bytes);
    if (parsed.error.has_value()) {
        std::cerr << "Feed parse failed at offset " << parsed.error_offset << ": "
                  << to_string(*parsed.error);

        if (parsed.parse_error.has_value()) {
            std::cerr << " (" << to_string(*parsed.parse_error) << ')';
        }

        std::cerr << '\n';
        std::cerr << "Messages parsed before failure: " << parsed.messages.size() << '\n';
        return 1;
    }

    flux::MatchingEngine engine;
    flux::itch::ReplayHandler replay{engine};
    const auto summary = replay.apply_all(parsed.messages);

    std::cout << "Flux ITCH replay\n";
    std::cout << "File: " << argv[1] << '\n';
    std::cout << "Bytes: " << bytes->size() << '\n';
    std::cout << "Messages parsed: " << parsed.messages.size() << '\n';
    std::cout << "Symbols: " << engine.symbol_count() << '\n';
    std::cout << '\n';
    std::cout << "Replay summary\n";
    std::cout << "  Added: " << summary.added << '\n';
    std::cout << "  Executed: " << summary.executed << '\n';
    std::cout << "  Canceled: " << summary.canceled << '\n';
    std::cout << "  Deleted: " << summary.deleted << '\n';
    std::cout << "  Replaced: " << summary.replaced << '\n';
    std::cout << "  Ignored: " << summary.ignored << '\n';
    std::cout << "  Rejected: " << summary.rejected << '\n';
    std::cout << "  Unknown orders: " << summary.unknown_orders << '\n';

    return 0;
}
