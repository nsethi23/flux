#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "flux/order_book.hpp"

namespace flux {

class MatchingEngine {
public:
    OrderBook& book_for(std::string_view symbol);

    [[nodiscard]] const OrderBook* find_book(std::string_view symbol) const;
    [[nodiscard]] std::size_t symbol_count() const;

private:
    std::unordered_map<std::string, OrderBook> books_by_symbol_;
};

}  // namespace flux
