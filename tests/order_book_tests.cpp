#include "flux/order_book.hpp"
#include "flux/matching_engine.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

struct RecordingListener : flux::BookListener {
    std::vector<flux::Trade> trades;
    std::vector<flux::TopOfBook> top_changes;

    void on_trade(const flux::Trade& trade) override {
        trades.push_back(trade);
    }

    void on_top_of_book_change(const flux::TopOfBook& top_of_book) override {
        top_changes.push_back(top_of_book);
    }
};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_empty_book_has_no_best_prices() {
    flux::OrderBook book;

    expect(!book.best_bid().has_value(), "empty book has no best bid");
    expect(!book.best_ask().has_value(), "empty book has no best ask");
    expect(book.order_count() == 0, "empty book has zero orders");
}

void test_rejects_zero_quantity_order() {
    flux::OrderBook book;

    const auto result = book.add_limit_order(
        {.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 0}
    );

    expect(!result.accepted, "zero quantity order is rejected");
    expect(
        result.reject_reason == flux::AddOrderRejectReason::ZeroQuantity,
        "zero quantity reject reason is reported"
    );
    expect(book.order_count() == 0, "rejected order is not stored");
}

void test_rejects_duplicate_order_id() {
    flux::OrderBook book;

    const auto first = book.add_limit_order(
        {.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100}
    );
    const auto second = book.add_limit_order(
        {.id = 1, .side = flux::Side::Sell, .price = 10'100, .quantity = 100}
    );

    expect(first.accepted, "first order with id is accepted");
    expect(!second.accepted, "duplicate order id is rejected");
    expect(
        second.reject_reason == flux::AddOrderRejectReason::DuplicateOrderId,
        "duplicate order id reject reason is reported"
    );
    expect(book.order_count() == 1, "duplicate order is not stored");
}

void test_best_bid_uses_highest_buy_price() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Buy, .price = 10'200, .quantity = 100});
    book.add_limit_order({.id = 3, .side = flux::Side::Buy, .price = 10'100, .quantity = 100});

    expect(book.best_bid() == std::optional<flux::Price>{10'200}, "best bid is highest buy price");
}

void test_best_ask_uses_lowest_sell_price() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'300, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Sell, .price = 10'100, .quantity = 100});
    book.add_limit_order({.id = 3, .side = flux::Side::Sell, .price = 10'200, .quantity = 100});

    expect(book.best_ask() == std::optional<flux::Price>{10'100}, "best ask is lowest sell price");
}

void test_fifo_ordering_within_price_level() {
    flux::OrderBook book;

    book.add_limit_order({.id = 10, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 11, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 12, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});

    const std::vector<flux::OrderId> expected{10, 11, 12};

    expect(
        book.order_ids_at_price(flux::Side::Buy, 10'000) == expected,
        "orders at same price preserve FIFO order"
    );
}

void test_buy_limit_order_fully_matches_resting_sell() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 100});

    const auto result = book.add_limit_order(
        {.id = 2, .side = flux::Side::Buy, .price = 10'100, .quantity = 100}
    );

    expect(result.accepted, "marketable buy limit order is accepted");
    expect(result.remaining_quantity == 0, "fully matched buy has no remaining quantity");
    expect(result.trades.size() == 1, "fully matched buy creates one trade");
    expect(result.trades[0].resting_order_id == 1, "trade references resting sell order");
    expect(result.trades[0].incoming_order_id == 2, "trade references incoming buy order");
    expect(result.trades[0].price == 10'000, "trade executes at resting order price");
    expect(result.trades[0].quantity == 100, "trade quantity equals matched quantity");
    expect(book.order_count() == 0, "fully filled resting order is removed");
    expect(!book.best_ask().has_value(), "empty ask level is removed");
}

void test_buy_limit_order_partially_fills_resting_sell() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 100});

    const auto result = book.add_limit_order(
        {.id = 2, .side = flux::Side::Buy, .price = 10'000, .quantity = 40}
    );

    expect(result.accepted, "partial buy match is accepted");
    expect(result.remaining_quantity == 0, "incoming buy is fully consumed");
    expect(result.trades.size() == 1, "partial buy match creates one trade");
    expect(result.trades[0].quantity == 40, "trade records incoming buy quantity");
    expect(book.order_count() == 1, "partially filled resting sell remains open");
    expect(book.best_ask() == std::optional<flux::Price>{10'000}, "remaining sell stays at ask");
}

void test_buy_limit_order_rests_unfilled_remainder() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 40});

    const auto result = book.add_limit_order(
        {.id = 2, .side = flux::Side::Buy, .price = 10'000, .quantity = 100}
    );

    const std::vector<flux::OrderId> expected{2};

    expect(result.accepted, "buy with remainder is accepted");
    expect(result.remaining_quantity == 60, "unfilled buy quantity remains");
    expect(result.trades.size() == 1, "buy with remainder creates one trade");
    expect(result.trades[0].quantity == 40, "trade consumes full resting sell quantity");
    expect(book.order_count() == 1, "unfilled buy remainder rests on book");
    expect(book.best_bid() == std::optional<flux::Price>{10'000}, "buy remainder becomes best bid");
    expect(book.order_ids_at_price(flux::Side::Buy, 10'000) == expected, "buy remainder uses incoming id");
}

void test_ioc_limit_order_does_not_rest_remainder() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 40});

    const auto result = book.add_limit_order(
        {
            .id = 2,
            .side = flux::Side::Buy,
            .price = 10'000,
            .quantity = 100,
            .time_in_force = flux::TimeInForce::ImmediateOrCancel,
        }
    );

    expect(result.accepted, "IOC limit order is accepted");
    expect(result.remaining_quantity == 60, "IOC reports unfilled remainder");
    expect(result.trades.size() == 1, "IOC trades available liquidity");
    expect(book.order_count() == 0, "IOC remainder does not rest on book");
    expect(!book.best_bid().has_value(), "IOC remainder does not create bid");
}

void test_fok_limit_order_rejects_when_not_fully_fillable() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 40});

    const auto result = book.add_limit_order(
        {
            .id = 2,
            .side = flux::Side::Buy,
            .price = 10'000,
            .quantity = 100,
            .time_in_force = flux::TimeInForce::FillOrKill,
        }
    );

    expect(!result.accepted, "FOK rejects when full quantity is unavailable");
    expect(
        result.reject_reason == flux::AddOrderRejectReason::FillOrKillNotFilled,
        "FOK reject reason is reported"
    );
    expect(book.order_count() == 1, "failed FOK does not mutate book");
    expect(book.order_status(1)->quantity == 40, "failed FOK leaves resting order unchanged");
}

void test_fok_limit_order_fills_when_enough_liquidity_exists() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 40});
    book.add_limit_order({.id = 2, .side = flux::Side::Sell, .price = 10'100, .quantity = 60});

    const auto result = book.add_limit_order(
        {
            .id = 3,
            .side = flux::Side::Buy,
            .price = 10'100,
            .quantity = 100,
            .time_in_force = flux::TimeInForce::FillOrKill,
        }
    );

    expect(result.accepted, "FOK fills when full quantity is available");
    expect(result.remaining_quantity == 0, "filled FOK has no remainder");
    expect(result.trades.size() == 2, "FOK can fill across price levels");
    expect(book.order_count() == 0, "filled FOK consumes resting liquidity and does not rest");
}

void test_sell_limit_order_matches_best_bid_first() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Buy, .price = 10'100, .quantity = 100});

    const auto result = book.add_limit_order(
        {.id = 3, .side = flux::Side::Sell, .price = 10'000, .quantity = 100}
    );

    expect(result.accepted, "marketable sell limit order is accepted");
    expect(result.trades.size() == 1, "sell match creates one trade");
    expect(result.trades[0].resting_order_id == 2, "sell matches highest bid first");
    expect(result.trades[0].price == 10'100, "sell trade executes at resting bid price");
    expect(book.best_bid() == std::optional<flux::Price>{10'000}, "lower bid remains after best bid fills");
}

void test_cancel_unknown_order_id_returns_false() {
    flux::OrderBook book;

    expect(!book.cancel_order(42), "cancel unknown order id returns false");
    expect(book.order_count() == 0, "cancel unknown order does not change book");
}

void test_cancel_removes_order_from_fifo_level() {
    flux::OrderBook book;

    book.add_limit_order({.id = 10, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 11, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 12, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});

    const std::vector<flux::OrderId> expected{10, 12};

    expect(book.cancel_order(11), "cancel existing order returns true");
    expect(book.order_count() == 2, "cancel removes order from id map");
    expect(
        book.order_ids_at_price(flux::Side::Buy, 10'000) == expected,
        "cancel removes order from FIFO level"
    );
}

void test_cancel_removes_empty_best_bid_level() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Buy, .price = 10'100, .quantity = 100});

    expect(book.cancel_order(2), "cancel best bid order succeeds");
    expect(book.best_bid() == std::optional<flux::Price>{10'000}, "next bid becomes best bid");
}

void test_cancel_removes_empty_best_ask_level() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'100, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Sell, .price = 10'000, .quantity = 100});

    expect(book.cancel_order(2), "cancel best ask order succeeds");
    expect(book.best_ask() == std::optional<flux::Price>{10'100}, "next ask becomes best ask");
}

void test_market_buy_consumes_asks_across_price_levels() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 40});
    book.add_limit_order({.id = 2, .side = flux::Side::Sell, .price = 10'100, .quantity = 60});
    book.add_limit_order({.id = 3, .side = flux::Side::Sell, .price = 10'200, .quantity = 100});

    const auto result = book.add_market_order(
        {.id = 4, .side = flux::Side::Buy, .price = 0, .quantity = 150}
    );

    expect(result.accepted, "market buy is accepted");
    expect(result.remaining_quantity == 0, "market buy fully fills when enough asks exist");
    expect(result.trades.size() == 3, "market buy can trade across ask levels");
    expect(result.trades[0].resting_order_id == 1, "market buy matches lowest ask first");
    expect(result.trades[1].resting_order_id == 2, "market buy matches next ask second");
    expect(result.trades[2].resting_order_id == 3, "market buy partially matches third ask");
    expect(result.trades[2].quantity == 50, "market buy only takes needed quantity from third ask");
    expect(book.best_ask() == std::optional<flux::Price>{10'200}, "partially filled ask remains best ask");
    expect(book.order_count() == 1, "only partial resting ask remains");
}

void test_market_sell_consumes_bids_across_price_levels() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Buy, .price = 10'200, .quantity = 40});
    book.add_limit_order({.id = 3, .side = flux::Side::Buy, .price = 10'100, .quantity = 60});

    const auto result = book.add_market_order(
        {.id = 4, .side = flux::Side::Sell, .price = 0, .quantity = 75}
    );

    expect(result.accepted, "market sell is accepted");
    expect(result.remaining_quantity == 0, "market sell fully fills when enough bids exist");
    expect(result.trades.size() == 2, "market sell can trade across bid levels");
    expect(result.trades[0].resting_order_id == 2, "market sell matches highest bid first");
    expect(result.trades[1].resting_order_id == 3, "market sell matches next highest bid second");
    expect(result.trades[1].quantity == 35, "market sell partially fills second bid level");
    expect(book.best_bid() == std::optional<flux::Price>{10'100}, "partial bid remains best bid");
    expect(book.order_count() == 2, "partial bid and lower bid remain");
}

void test_market_order_discards_unfilled_quantity() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 40});

    const auto result = book.add_market_order(
        {.id = 2, .side = flux::Side::Buy, .price = 0, .quantity = 100}
    );

    expect(result.accepted, "oversized market order is accepted");
    expect(result.remaining_quantity == 60, "unfilled market quantity is reported");
    expect(result.trades.size() == 1, "market order trades available liquidity");
    expect(book.order_count() == 0, "unfilled market quantity does not rest");
    expect(!book.best_bid().has_value(), "unfilled market buy does not become bid");
    expect(!book.best_ask().has_value(), "consumed ask is removed");
}

void test_market_order_rejects_duplicate_resting_order_id() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 40});

    const auto result = book.add_market_order(
        {.id = 1, .side = flux::Side::Buy, .price = 0, .quantity = 100}
    );

    expect(!result.accepted, "market order duplicate id is rejected");
    expect(book.order_count() == 1, "duplicate market order does not change book");
    expect(book.best_ask() == std::optional<flux::Price>{10'000}, "resting order remains after rejection");
}

void test_market_order_on_empty_book_reports_unfilled_quantity() {
    flux::OrderBook book;

    const auto result = book.add_market_order(
        {.id = 1, .side = flux::Side::Buy, .price = 0, .quantity = 100}
    );

    expect(result.accepted, "market order on empty book is accepted");
    expect(result.remaining_quantity == 100, "empty book market order reports full remaining quantity");
    expect(result.trades.empty(), "empty book market order creates no trades");
    expect(book.order_count() == 0, "empty book market order does not rest");
}

void test_duplicate_id_can_be_reused_after_full_fill() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});

    const auto result = book.add_limit_order(
        {.id = 1, .side = flux::Side::Buy, .price = 9'900, .quantity = 100}
    );

    expect(result.accepted, "order id can be reused after original order fully fills");
    expect(book.order_count() == 1, "reused id rests as a new order");
    expect(book.best_bid() == std::optional<flux::Price>{9'900}, "reused id order has new price");
}

void test_duplicate_id_can_be_reused_after_cancel() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.cancel_order(1);

    const auto result = book.add_limit_order(
        {.id = 1, .side = flux::Side::Sell, .price = 10'100, .quantity = 100}
    );

    expect(result.accepted, "order id can be reused after cancel");
    expect(book.best_ask() == std::optional<flux::Price>{10'100}, "reused id order rests on ask side");
}

void test_partial_fill_preserves_fifo_for_remaining_order() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Sell, .price = 10'000, .quantity = 100});

    book.add_limit_order({.id = 3, .side = flux::Side::Buy, .price = 10'000, .quantity = 40});

    const std::vector<flux::OrderId> expected{1, 2};

    expect(
        book.order_ids_at_price(flux::Side::Sell, 10'000) == expected,
        "partially filled resting order keeps FIFO position"
    );
    expect(book.order_status(1)->quantity == 60, "partially filled resting order keeps remaining quantity");
}

void test_reduce_unknown_order_id_returns_false() {
    flux::OrderBook book;

    expect(!book.reduce_order_quantity(42, 10), "reduce unknown order id returns false");
    expect(book.order_count() == 0, "reduce unknown order does not change book");
}

void test_reduce_zero_quantity_returns_false() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});

    expect(!book.reduce_order_quantity(1, 0), "zero quantity reduction is rejected");
    expect(book.order_status(1)->quantity == 100, "zero quantity reduction does not change order");
}

void test_reduce_order_quantity_partially() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});

    expect(book.reduce_order_quantity(1, 40), "partial quantity reduction succeeds");

    const auto status = book.order_status(1);

    expect(status.has_value(), "partially reduced order remains on book");
    expect(status->quantity == 60, "partial reduction lowers order quantity");
    expect(book.best_bid() == std::optional<flux::Price>{10'000}, "partially reduced order keeps price level");
    expect(book.order_count() == 1, "partial reduction does not remove order");
}

void test_reduce_order_quantity_to_zero_removes_order() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 100});

    expect(book.reduce_order_quantity(1, 100), "full quantity reduction succeeds");
    expect(!book.order_status(1).has_value(), "fully reduced order is removed");
    expect(!book.best_ask().has_value(), "empty ask level is removed after full reduction");
    expect(book.order_count() == 0, "full reduction removes order from id map");
}

void test_reduce_more_than_remaining_removes_order() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});

    expect(book.reduce_order_quantity(1, 150), "oversized quantity reduction succeeds");
    expect(!book.order_status(1).has_value(), "oversized reduction removes order");
    expect(!book.best_bid().has_value(), "empty bid level is removed after oversized reduction");
    expect(book.order_count() == 0, "oversized reduction removes order from id map");
}

void test_reduce_order_quantity_preserves_fifo_position() {
    flux::OrderBook book;

    book.add_limit_order({.id = 10, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 11, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 12, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});

    const std::vector<flux::OrderId> expected{10, 11, 12};

    expect(book.reduce_order_quantity(11, 40), "middle order quantity reduction succeeds");
    expect(
        book.order_ids_at_price(flux::Side::Buy, 10'000) == expected,
        "quantity reduction preserves FIFO position"
    );
}

void test_replace_order_resets_priority() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});

    const std::vector<flux::OrderId> expected{2, 3};

    const auto result = book.replace_order(
        1,
        {.id = 3, .side = flux::Side::Buy, .price = 10'000, .quantity = 50}
    );

    expect(result.accepted, "replace existing order succeeds");
    expect(book.order_ids_at_price(flux::Side::Buy, 10'000) == expected, "replacement loses old FIFO priority");
    expect(!book.order_status(1).has_value(), "replaced order id is removed");
    expect(book.order_status(3)->quantity == 50, "replacement order is inserted");
}

void test_replace_unknown_order_returns_false() {
    flux::OrderBook book;

    const auto result = book.replace_order(
        42,
        {.id = 43, .side = flux::Side::Buy, .price = 10'000, .quantity = 50}
    );

    expect(!result.accepted, "replace unknown order returns false");
    expect(book.order_count() == 0, "replace unknown order does not change book");
}

void test_replace_with_duplicate_replacement_id_returns_false() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Buy, .price = 10'100, .quantity = 100});

    const auto result = book.replace_order(
        1,
        {.id = 2, .side = flux::Side::Sell, .price = 10'200, .quantity = 50}
    );

    expect(!result.accepted, "replace with duplicate replacement id returns false");
    expect(book.order_status(1).has_value(), "failed replace keeps original order");
    expect(book.order_status(2).has_value(), "failed replace keeps conflicting order");
}

void test_replace_order_returns_trades_when_marketable() {
    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 9'900, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Sell, .price = 10'000, .quantity = 100});

    const auto result = book.replace_order(
        1,
        {.id = 3, .side = flux::Side::Buy, .price = 10'000, .quantity = 100}
    );

    expect(result.accepted, "marketable replace succeeds");
    expect(result.trades.size() == 1, "marketable replace returns trade");
    expect(result.trades[0].resting_order_id == 2, "marketable replace trades against resting ask");
    expect(book.order_count() == 0, "marketable replace removes filled orders");
}

void test_matching_engine_isolates_symbols() {
    flux::MatchingEngine engine;

    auto& aapl = engine.book_for("AAPL");
    auto& msft = engine.book_for("MSFT");

    aapl.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    msft.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 20'000, .quantity = 100});

    expect(engine.symbol_count() == 2, "matching engine tracks two symbols");
    expect(engine.find_book("AAPL")->best_bid() == std::optional<flux::Price>{10'000}, "AAPL book has AAPL bid");
    expect(engine.find_book("MSFT")->best_ask() == std::optional<flux::Price>{20'000}, "MSFT book has MSFT ask");
    expect(engine.find_book("GOOG") == nullptr, "unknown symbol returns no book");
}

void test_listener_receives_trade_and_top_of_book_changes() {
    flux::OrderBook book;
    RecordingListener listener;
    book.set_listener(&listener);

    book.add_limit_order({.id = 1, .side = flux::Side::Sell, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});

    expect(listener.trades.size() == 1, "listener receives trade event");
    expect(listener.trades[0].quantity == 100, "listener trade has matched quantity");
    expect(listener.top_changes.size() == 2, "listener receives top-of-book changes");
    expect(listener.top_changes[0].ask == std::optional<flux::Price>{10'000}, "listener sees initial ask");
    expect(listener.top_changes[0].ask_quantity == 100, "listener sees initial ask quantity");
    expect(!listener.top_changes[1].ask.has_value(), "listener sees ask removed after trade");
}

void test_listener_receives_top_of_book_quantity_change() {
    flux::OrderBook book;
    RecordingListener listener;
    book.set_listener(&listener);

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.reduce_order_quantity(1, 40);

    expect(listener.top_changes.size() == 2, "listener receives quantity-only top-of-book change");
    expect(listener.top_changes[1].bid == std::optional<flux::Price>{10'000}, "quantity change keeps bid price");
    expect(listener.top_changes[1].bid_quantity == 60, "quantity change reports updated bid quantity");
}

}  // namespace

int main() {
    test_empty_book_has_no_best_prices();
    test_rejects_zero_quantity_order();
    test_rejects_duplicate_order_id();
    test_best_bid_uses_highest_buy_price();
    test_best_ask_uses_lowest_sell_price();
    test_fifo_ordering_within_price_level();
    test_buy_limit_order_fully_matches_resting_sell();
    test_buy_limit_order_partially_fills_resting_sell();
    test_buy_limit_order_rests_unfilled_remainder();
    test_ioc_limit_order_does_not_rest_remainder();
    test_fok_limit_order_rejects_when_not_fully_fillable();
    test_fok_limit_order_fills_when_enough_liquidity_exists();
    test_sell_limit_order_matches_best_bid_first();
    test_cancel_unknown_order_id_returns_false();
    test_cancel_removes_order_from_fifo_level();
    test_cancel_removes_empty_best_bid_level();
    test_cancel_removes_empty_best_ask_level();
    test_market_buy_consumes_asks_across_price_levels();
    test_market_sell_consumes_bids_across_price_levels();
    test_market_order_discards_unfilled_quantity();
    test_market_order_rejects_duplicate_resting_order_id();
    test_market_order_on_empty_book_reports_unfilled_quantity();
    test_duplicate_id_can_be_reused_after_full_fill();
    test_duplicate_id_can_be_reused_after_cancel();
    test_partial_fill_preserves_fifo_for_remaining_order();
    test_reduce_unknown_order_id_returns_false();
    test_reduce_zero_quantity_returns_false();
    test_reduce_order_quantity_partially();
    test_reduce_order_quantity_to_zero_removes_order();
    test_reduce_more_than_remaining_removes_order();
    test_reduce_order_quantity_preserves_fifo_position();
    test_replace_order_resets_priority();
    test_replace_unknown_order_returns_false();
    test_replace_with_duplicate_replacement_id_returns_false();
    test_replace_order_returns_trades_when_marketable();
    test_matching_engine_isolates_symbols();
    test_listener_receives_trade_and_top_of_book_changes();
    test_listener_receives_top_of_book_quantity_change();

    if (failures != 0) {
        std::cerr << failures << " test failure(s)\n";
        return EXIT_FAILURE;
    }

    std::cout << "All order book tests passed\n";
    return EXIT_SUCCESS;
}
