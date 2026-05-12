#include "flux/order_book.hpp"

#include <algorithm>
#include <functional>

namespace flux {

AddOrderResult OrderBook::add_limit_order(Order order) {
    if (order.quantity == 0) {
        return {.accepted = false};
    }

    if (orders_by_id_.contains(order.id)) {
        return {.accepted = false};
    }

    if (order.side == Side::Buy) {
        AddOrderResult result{.accepted = true};
        match_buy_order(order, result.trades);
        result.remaining_quantity = order.quantity;

        if (order.quantity > 0) {
            rest_order(order);
        }

        return result;
    }

    AddOrderResult result{.accepted = true};
    match_sell_order(order, result.trades);
    result.remaining_quantity = order.quantity;

    if (order.quantity > 0) {
        rest_order(order);
    }

    return result;
}

void OrderBook::rest_order(Order order) {
    const auto [it, inserted] = orders_by_id_.emplace(order.id, order);
    (void)inserted;

    if (order.side == Side::Buy) {
        bids_[order.price].fifo_order_ids.push_back(it->first);
    } else {
        asks_[order.price].fifo_order_ids.push_back(it->first);
    }
}

void OrderBook::match_buy_order(Order& incoming, std::vector<Trade>& trades) {
    while (incoming.quantity > 0 && !asks_.empty()) {
        auto best_ask = asks_.begin();
        if (best_ask->first > incoming.price) {
            break;
        }

        auto& fifo = best_ask->second.fifo_order_ids;
        const OrderId resting_id = fifo.front();
        auto resting = orders_by_id_.find(resting_id);

        const Quantity trade_quantity = std::min(incoming.quantity, resting->second.quantity);
        trades.push_back(
            {
                .resting_order_id = resting_id,
                .incoming_order_id = incoming.id,
                .price = resting->second.price,
                .quantity = trade_quantity,
            }
        );

        incoming.quantity -= trade_quantity;
        resting->second.quantity -= trade_quantity;

        if (resting->second.quantity == 0) {
            orders_by_id_.erase(resting);
            fifo.pop_front();

            if (fifo.empty()) {
                asks_.erase(best_ask);
            }
        }
    }
}

void OrderBook::match_sell_order(Order& incoming, std::vector<Trade>& trades) {
    while (incoming.quantity > 0 && !bids_.empty()) {
        auto best_bid = bids_.begin();
        if (best_bid->first < incoming.price) {
            break;
        }

        auto& fifo = best_bid->second.fifo_order_ids;
        const OrderId resting_id = fifo.front();
        auto resting = orders_by_id_.find(resting_id);

        const Quantity trade_quantity = std::min(incoming.quantity, resting->second.quantity);
        trades.push_back(
            {
                .resting_order_id = resting_id,
                .incoming_order_id = incoming.id,
                .price = resting->second.price,
                .quantity = trade_quantity,
            }
        );

        incoming.quantity -= trade_quantity;
        resting->second.quantity -= trade_quantity;

        if (resting->second.quantity == 0) {
            orders_by_id_.erase(resting);
            fifo.pop_front();

            if (fifo.empty()) {
                bids_.erase(best_bid);
            }
        }
    }
}

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }

    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }

    return asks_.begin()->first;
}

std::vector<OrderId> OrderBook::order_ids_at_price(Side side, Price price) const {
    if (side == Side::Buy) {
        const auto level = bids_.find(price);
        if (level == bids_.end()) {
            return {};
        }

        return {level->second.fifo_order_ids.begin(), level->second.fifo_order_ids.end()};
    }

    const auto level = asks_.find(price);
    if (level == asks_.end()) {
        return {};
    }

    return {level->second.fifo_order_ids.begin(), level->second.fifo_order_ids.end()};
}

std::size_t OrderBook::order_count() const {
    return orders_by_id_.size();
}

}  // namespace flux
