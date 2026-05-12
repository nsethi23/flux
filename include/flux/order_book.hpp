#pragma once

#include <deque>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "flux/order.hpp"

namespace flux {

struct Trade {
    OrderId resting_order_id{};
    OrderId incoming_order_id{};
    Price price{};
    Quantity quantity{};
};

struct AddOrderResult {
    bool accepted{};
    Quantity remaining_quantity{};
    std::vector<Trade> trades;
};

class OrderBook {
public:
    AddOrderResult add_limit_order(Order order);
    AddOrderResult add_market_order(Order order);
    bool cancel_order(OrderId order_id);

    [[nodiscard]] std::optional<Price> best_bid() const;
    [[nodiscard]] std::optional<Price> best_ask() const;
    [[nodiscard]] std::vector<OrderId> order_ids_at_price(Side side, Price price) const;
    [[nodiscard]] std::size_t order_count() const;

private:
    struct PriceLevel {
        std::deque<OrderId> fifo_order_ids;
    };

    void rest_order(Order order);
    void match_buy_order(Order& incoming, std::vector<Trade>& trades, bool enforce_price_limit);
    void match_sell_order(Order& incoming, std::vector<Trade>& trades, bool enforce_price_limit);
    void remove_from_level(Side side, Price price, OrderId order_id);

    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    std::map<Price, PriceLevel, std::less<Price>> asks_;
    std::unordered_map<OrderId, Order> orders_by_id_;
};

}  // namespace flux
