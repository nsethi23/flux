#include <iomanip>
#include <iostream>
#include <string_view>

#include "flux/order.hpp"
#include "flux/order_book.hpp"
#include "flux/version.hpp"

namespace {

flux::OrderId gNextId = 1;
flux::OrderId next_id() { return gNextId++; }

void section(std::string_view title) {
    std::cout << "\n── " << title << " ──\n\n";
}

// Sum all quantities resting at a given price level.
flux::Quantity level_qty(const flux::OrderBook& book, flux::Side side, flux::Price price) {
    flux::Quantity total = 0;
    for (auto id : book.order_ids_at_price(side, price)) {
        total += book.order_status(id)->quantity;
    }
    return total;
}

// Print one row of the order book display.
void row(std::string_view side, flux::Price price, flux::Quantity qty) {
    std::cout << "    " << side << "  " << std::setw(6) << price
              << "  │  " << std::setw(5) << qty << '\n';
}

// Print the top two levels on each side. Levels with zero quantity are omitted.
void print_book(
    const flux::OrderBook& book,
    flux::Price ask_far, flux::Price ask_near,
    flux::Price bid_near, flux::Price bid_far
) {
    for (auto [p, s] : {std::pair{ask_far, "ask"}, {ask_near, "ask"}}) {
        if (auto q = level_qty(book, flux::Side::Sell, p); q > 0) row(s, p, q);
    }
    std::cout << "    ─────────────────────────\n";
    for (auto [p, s] : {std::pair{bid_near, "bid"}, {bid_far, "bid"}}) {
        if (auto q = level_qty(book, flux::Side::Buy, p); q > 0) row(s, p, q);
    }
}

// Listener that prints each event as it fires.
class PrintListener : public flux::BookListener {
public:
    void on_trade(const flux::Trade& t) override {
        std::cout << "  [trade]        qty=" << std::setw(4) << t.quantity
                  << "  price=" << t.price << '\n';
    }

    void on_top_of_book_change(const flux::TopOfBook& top) override {
        std::cout << "  [top-of-book]  bid=";
        if (top.bid) std::cout << *top.bid << " x" << top.bid_quantity;
        else         std::cout << "none             ";
        std::cout << "   ask=";
        if (top.ask) std::cout << *top.ask << " x" << top.ask_quantity;
        else         std::cout << "none";
        std::cout << '\n';
    }
};

}  // namespace

int main() {
    std::cout << "Flux matching engine v" << flux::kVersion << '\n';

    // ── Scenario 1: Limit order crosses two price levels ─────────────────────
    section("Scenario 1 — Crossing limit order (price-time priority)");
    {
        flux::OrderBook book;

        book.add_limit_order({.id = next_id(), .side = flux::Side::Sell, .price = 10'050, .quantity = 75});
        book.add_limit_order({.id = next_id(), .side = flux::Side::Sell, .price = 10'000, .quantity = 150});
        book.add_limit_order({.id = next_id(), .side = flux::Side::Buy,  .price =  9'950, .quantity = 100});
        book.add_limit_order({.id = next_id(), .side = flux::Side::Buy,  .price =  9'900, .quantity = 80});

        std::cout << "  Resting book:\n";
        print_book(book, 10'050, 10'000, 9'950, 9'900);

        // Aggressive buy priced to sweep the best ask and bite into the next.
        std::cout << "\n  Incoming: Buy 200 @ 10050  (matches both ask levels)\n";
        const auto r = book.add_limit_order(
            {.id = next_id(), .side = flux::Side::Buy, .price = 10'050, .quantity = 200}
        );
        for (const auto& t : r.trades) {
            std::cout << "    trade: " << t.quantity << " @ " << t.price << '\n';
        }
        std::cout << "    remaining: " << r.remaining_quantity << "\n";

        std::cout << "\n  Book after:\n";
        print_book(book, 10'050, 10'000, 9'950, 9'900);
    }

    // ── Scenario 2: IOC ───────────────────────────────────────────────────────
    section("Scenario 2 — IOC: fill what's available, cancel the rest");
    {
        flux::OrderBook book;
        book.add_limit_order({.id = next_id(), .side = flux::Side::Buy, .price = 9'950, .quantity = 100});

        std::cout << "  Resting bid: 9950 x 100\n";
        std::cout << "  Incoming: Sell 250 @ 9950  [IOC]\n";

        const auto r = book.add_limit_order({
            .id             = next_id(),
            .side           = flux::Side::Sell,
            .price          = 9'950,
            .quantity       = 250,
            .time_in_force  = flux::TimeInForce::ImmediateOrCancel,
        });

        std::cout << "    trade: " << r.trades[0].quantity << " @ " << r.trades[0].price << '\n';
        std::cout << "    remaining: " << r.remaining_quantity
                  << "  cancelled — IOC does not rest\n";
        std::cout << "  Orders on book: " << book.order_count() << '\n';
    }

    // ── Scenario 3: FOK ───────────────────────────────────────────────────────
    section("Scenario 3 — FOK: fill everything or nothing");
    {
        flux::OrderBook book;
        book.add_limit_order({.id = next_id(), .side = flux::Side::Sell, .price = 10'000, .quantity = 50});

        std::cout << "  Resting ask: 10000 x 50\n";

        std::cout << "  Incoming: Buy 200 @ 10000  [FOK]  (need 200, only 50 available)\n";
        const auto r1 = book.add_limit_order({
            .id             = next_id(),
            .side           = flux::Side::Buy,
            .price          = 10'000,
            .quantity       = 200,
            .time_in_force  = flux::TimeInForce::FillOrKill,
        });
        std::cout << "    " << (r1.accepted ? "accepted" : "rejected")
                  << " — book unchanged, " << book.order_count() << " order(s) resting\n";

        std::cout << "  Incoming: Buy  50 @ 10000  [FOK]  (exactly 50 available)\n";
        const auto r2 = book.add_limit_order({
            .id             = next_id(),
            .side           = flux::Side::Buy,
            .price          = 10'000,
            .quantity       = 50,
            .time_in_force  = flux::TimeInForce::FillOrKill,
        });
        std::cout << "    trade: " << r2.trades[0].quantity << " @ " << r2.trades[0].price << '\n';
        std::cout << "  Orders on book: " << book.order_count() << '\n';
    }

    // ── Scenario 4: BookListener ──────────────────────────────────────────────
    section("Scenario 4 — BookListener: real-time event callbacks");
    {
        flux::OrderBook book;
        PrintListener listener;
        book.set_listener(&listener);

        std::cout << "  Adding sell 100 @ 10000:\n";
        book.add_limit_order({.id = next_id(), .side = flux::Side::Sell, .price = 10'000, .quantity = 100});

        std::cout << "\n  Adding buy 60 @ 10000 (partial match):\n";
        book.add_limit_order({.id = next_id(), .side = flux::Side::Buy, .price = 10'000, .quantity = 60});

        std::cout << "\n  Cancelling remaining 40 @ 10000:\n";
        const auto ids = book.order_ids_at_price(flux::Side::Sell, 10'000);
        if (!ids.empty()) {
            book.cancel_order(ids[0]);
        }
    }

    std::cout << "\nRun ./flux_bench for nanosecond latency measurements.\n";
    return 0;
}
