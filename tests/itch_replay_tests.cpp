#include "flux/itch_replay.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

int failures = 0;

flux::itch::MessageHeader header() {
    return {.stock_locate = 1, .tracking_number = 1, .timestamp = 1};
}

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_replay_add_order_routes_to_symbol_book() {
    flux::MatchingEngine engine;
    flux::itch::ReplayHandler replay{engine};

    const auto result = replay.apply(
        flux::itch::AddOrder{
            .header = header(),
            .order_id = 100,
            .side = flux::Side::Buy,
            .quantity = 50,
            .stock = "AAPL",
            .price = 18'7500,
        }
    );

    const auto* book = engine.find_book("AAPL");

    expect(result.action == flux::itch::ReplayAction::Added, "add order replay returns Added");
    expect(book != nullptr, "add order creates symbol book");
    expect(book->best_bid() == std::optional<flux::Price>{18'7500}, "add order creates bid");
    expect(book->order_status(100)->quantity == 50, "add order stores quantity");
}

void test_replay_execute_reduces_quantity() {
    flux::MatchingEngine engine;
    flux::itch::ReplayHandler replay{engine};

    replay.apply(
        flux::itch::AddOrder{
            .header = header(),
            .order_id = 100,
            .side = flux::Side::Sell,
            .quantity = 50,
            .stock = "MSFT",
            .price = 40'0000,
        }
    );

    const auto result = replay.apply(
        flux::itch::OrderExecuted{
            .header = header(),
            .order_id = 100,
            .executed_quantity = 20,
            .match_number = 900,
        }
    );

    const auto* book = engine.find_book("MSFT");

    expect(result.action == flux::itch::ReplayAction::Executed, "execution replay returns Executed");
    expect(book->order_status(100)->quantity == 30, "execution reduces resting quantity");
}

void test_replay_cancel_reduces_quantity() {
    flux::MatchingEngine engine;
    flux::itch::ReplayHandler replay{engine};

    replay.apply(
        flux::itch::AddOrder{
            .header = header(),
            .order_id = 100,
            .side = flux::Side::Buy,
            .quantity = 50,
            .stock = "AAPL",
            .price = 18'7500,
        }
    );

    const auto result = replay.apply(
        flux::itch::OrderCancel{
            .header = header(),
            .order_id = 100,
            .canceled_quantity = 10,
        }
    );

    const auto* book = engine.find_book("AAPL");

    expect(result.action == flux::itch::ReplayAction::Canceled, "cancel replay returns Canceled");
    expect(book->order_status(100)->quantity == 40, "cancel reduces resting quantity");
}

void test_replay_delete_removes_order() {
    flux::MatchingEngine engine;
    flux::itch::ReplayHandler replay{engine};

    replay.apply(
        flux::itch::AddOrder{
            .header = header(),
            .order_id = 100,
            .side = flux::Side::Buy,
            .quantity = 50,
            .stock = "AAPL",
            .price = 18'7500,
        }
    );

    const auto result = replay.apply(
        flux::itch::OrderDelete{
            .header = header(),
            .order_id = 100,
        }
    );

    const auto* book = engine.find_book("AAPL");

    expect(result.action == flux::itch::ReplayAction::Deleted, "delete replay returns Deleted");
    expect(!book->order_status(100).has_value(), "delete removes order");
    expect(!book->best_bid().has_value(), "delete removes empty price level");
}

void test_replay_unknown_order_returns_unknown_order() {
    flux::MatchingEngine engine;
    flux::itch::ReplayHandler replay{engine};

    const auto result = replay.apply(
        flux::itch::OrderDelete{
            .header = header(),
            .order_id = 404,
        }
    );

    expect(result.action == flux::itch::ReplayAction::UnknownOrder, "unknown replay order is reported");
    expect(engine.symbol_count() == 0, "unknown replay order does not create symbol book");
}

void test_replay_forgets_fully_removed_order() {
    flux::MatchingEngine engine;
    flux::itch::ReplayHandler replay{engine};

    replay.apply(
        flux::itch::AddOrder{
            .header = header(),
            .order_id = 100,
            .side = flux::Side::Sell,
            .quantity = 50,
            .stock = "AAPL",
            .price = 18'7500,
        }
    );

    replay.apply(
        flux::itch::OrderExecuted{
            .header = header(),
            .order_id = 100,
            .executed_quantity = 50,
            .match_number = 900,
        }
    );

    const auto result = replay.apply(
        flux::itch::OrderDelete{
            .header = header(),
            .order_id = 100,
        }
    );

    expect(result.action == flux::itch::ReplayAction::UnknownOrder, "fully removed order is forgotten");
}

}  // namespace

int main() {
    test_replay_add_order_routes_to_symbol_book();
    test_replay_execute_reduces_quantity();
    test_replay_cancel_reduces_quantity();
    test_replay_delete_removes_order();
    test_replay_unknown_order_returns_unknown_order();
    test_replay_forgets_fully_removed_order();

    if (failures != 0) {
        std::cerr << failures << " test failure(s)\n";
        return EXIT_FAILURE;
    }

    std::cout << "All ITCH replay tests passed\n";
    return EXIT_SUCCESS;
}
