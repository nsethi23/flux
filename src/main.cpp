#include <iostream>

#include "flux/order.hpp"
#include "flux/order_book.hpp"
#include "flux/version.hpp"

int main() {
    std::cout << "Flux matching engine v" << flux::kVersion << '\n';

    flux::OrderBook book;

    book.add_limit_order({.id = 1, .side = flux::Side::Buy, .price = 10'000, .quantity = 100});
    book.add_limit_order({.id = 2, .side = flux::Side::Buy, .price = 10'100, .quantity = 50});
    book.add_limit_order({.id = 3, .side = flux::Side::Sell, .price = 10'300, .quantity = 75});

    std::cout << "Orders: " << book.order_count() << '\n';

    if (const auto best_bid = book.best_bid()) {
        std::cout << "Best bid: " << *best_bid << '\n';
    }

    if (const auto best_ask = book.best_ask()) {
        std::cout << "Best ask: " << *best_ask << '\n';
    }

    return 0;
}
