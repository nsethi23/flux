#include "flux/itch_replay.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

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

void test_replay_add_order_rests_without_matching() {
    flux::MatchingEngine engine;
    auto& book = engine.book_for("AAPL");
    book.add_resting_order({.id = 99, .side = flux::Side::Sell, .price = 18'0000, .quantity = 50});

    flux::itch::ReplayHandler replay{engine};
    const auto result = replay.apply(
        flux::itch::AddOrder{
            .header = header(),
            .order_id = 100,
            .side = flux::Side::Buy,
            .quantity = 50,
            .stock = "AAPL",
            .price = 19'0000,
        }
    );

    expect(result.action == flux::itch::ReplayAction::Added, "replay add returns Added");
    expect(book.order_status(99).has_value(), "replay add does not match existing crossed ask");
    expect(book.order_status(100).has_value(), "replay add rests incoming crossed bid");
    expect(book.best_bid() == std::optional<flux::Price>{19'0000}, "crossed replay bid rests on book");
    expect(book.best_ask() == std::optional<flux::Price>{18'0000}, "crossed replay ask remains on book");
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

void test_replay_executed_with_price_reduces_quantity() {
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
        flux::itch::OrderExecutedWithPrice{
            .header = header(),
            .order_id = 100,
            .executed_quantity = 20,
            .match_number = 900,
            .printable = true,
            .execution_price = 39'9900,
        }
    );

    const auto* book = engine.find_book("MSFT");

    expect(result.action == flux::itch::ReplayAction::Executed, "executed-with-price replay returns Executed");
    expect(book->order_status(100)->quantity == 30, "executed-with-price reduces resting quantity");
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

void test_replay_add_order_with_mpid_routes_to_symbol_book() {
    flux::MatchingEngine engine;
    flux::itch::ReplayHandler replay{engine};

    const auto result = replay.apply(
        flux::itch::AddOrderWithMpid{
            .header = header(),
            .order_id = 100,
            .side = flux::Side::Sell,
            .quantity = 50,
            .stock = "MSFT",
            .price = 40'0000,
            .attribution = "ABCD",
        }
    );

    const auto* book = engine.find_book("MSFT");

    expect(result.action == flux::itch::ReplayAction::Added, "MPID add replay returns Added");
    expect(book != nullptr, "MPID add creates symbol book");
    expect(book->best_ask() == std::optional<flux::Price>{40'0000}, "MPID add creates ask");
}

void test_replay_replace_order_updates_id_price_and_quantity() {
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
        flux::itch::OrderReplace{
            .header = header(),
            .original_order_id = 100,
            .new_order_id = 101,
            .quantity = 30,
            .price = 18'7600,
        }
    );

    const auto* book = engine.find_book("AAPL");

    expect(result.action == flux::itch::ReplayAction::Replaced, "replace replay returns Replaced");
    expect(!book->order_status(100).has_value(), "replace removes original id");
    expect(book->order_status(101)->quantity == 30, "replace inserts new quantity");
    expect(book->best_bid() == std::optional<flux::Price>{18'7600}, "replace updates price");
}

void test_replay_stock_directory_is_ignored() {
    flux::MatchingEngine engine;
    flux::itch::ReplayHandler replay{engine};

    const auto result = replay.apply(
        flux::itch::StockDirectory{
            .header = header(),
            .stock = "AAPL",
            .market_category = 'Q',
            .financial_status_indicator = 'N',
            .round_lot_size = 100,
            .round_lots_only = true,
        }
    );

    expect(result.action == flux::itch::ReplayAction::Ignored, "stock directory replay is ignored");
    expect(engine.symbol_count() == 0, "stock directory does not create book");
}

void test_replay_apply_all_reports_summary() {
    flux::MatchingEngine engine;
    flux::itch::ReplayHandler replay{engine};

    std::vector<flux::itch::FeedMessage> messages{
        {
            .offset = 2,
            .message =
                flux::itch::AddOrder{
                    .header = header(),
                    .order_id = 100,
                    .side = flux::Side::Buy,
                    .quantity = 50,
                    .stock = "AAPL",
                    .price = 18'7500,
                },
        },
        {
            .offset = 40,
            .message =
                flux::itch::OrderCancel{
                    .header = header(),
                    .order_id = 100,
                    .canceled_quantity = 10,
                },
        },
        {
            .offset = 65,
            .message =
                flux::itch::OrderDelete{
                    .header = header(),
                    .order_id = 404,
                },
        },
        {
            .offset = 90,
            .message =
                flux::itch::StockDirectory{
                    .header = header(),
                    .stock = "AAPL",
                    .market_category = 'Q',
                    .financial_status_indicator = 'N',
                    .round_lot_size = 100,
                    .round_lots_only = true,
                },
        },
    };

    const auto summary = replay.apply_all(messages);
    const auto* book = engine.find_book("AAPL");

    expect(summary.added == 1, "summary counts added messages");
    expect(summary.canceled == 1, "summary counts canceled messages");
    expect(summary.ignored == 1, "summary counts ignored messages");
    expect(summary.unknown_orders == 1, "summary counts unknown orders");
    expect(book->order_status(100)->quantity == 40, "apply_all updates book state");
}

void test_replay_replace_order_rests_without_matching() {
    flux::MatchingEngine engine;
    auto& book = engine.book_for("AAPL");
    book.add_resting_order({.id = 99, .side = flux::Side::Sell, .price = 18'0000, .quantity = 50});

    flux::itch::ReplayHandler replay{engine};
    replay.apply(
        flux::itch::AddOrder{
            .header = header(),
            .order_id = 100,
            .side = flux::Side::Buy,
            .quantity = 50,
            .stock = "AAPL",
            .price = 17'0000,
        }
    );

    const auto result = replay.apply(
        flux::itch::OrderReplace{
            .header = header(),
            .original_order_id = 100,
            .new_order_id = 101,
            .quantity = 50,
            .price = 19'0000,
        }
    );

    expect(result.action == flux::itch::ReplayAction::Replaced, "replace replay returns Replaced");
    expect(book.order_count() == 2, "replace does not trigger matching — both orders rest");
    expect(book.order_status(99)->quantity == 50, "existing sell unchanged");
    expect(book.order_status(101)->quantity == 50, "replacement buy rests without matching");
    expect(book.best_bid() == std::optional<flux::Price>{19'0000}, "replacement bid rests on book");
    expect(book.best_ask() == std::optional<flux::Price>{18'0000}, "existing ask remains on book");
}

}  // namespace

int main() {
    test_replay_add_order_routes_to_symbol_book();
    test_replay_add_order_rests_without_matching();
    test_replay_execute_reduces_quantity();
    test_replay_executed_with_price_reduces_quantity();
    test_replay_cancel_reduces_quantity();
    test_replay_add_order_with_mpid_routes_to_symbol_book();
    test_replay_replace_order_updates_id_price_and_quantity();
    test_replay_replace_order_rests_without_matching();
    test_replay_stock_directory_is_ignored();
    test_replay_delete_removes_order();
    test_replay_unknown_order_returns_unknown_order();
    test_replay_forgets_fully_removed_order();
    test_replay_apply_all_reports_summary();

    if (failures != 0) {
        std::cerr << failures << " test failure(s)\n";
        return EXIT_FAILURE;
    }

    std::cout << "All ITCH replay tests passed\n";
    return EXIT_SUCCESS;
}
