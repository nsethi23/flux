#include "flux/order_book.hpp"

#include <algorithm>
#include <functional>
#include <iterator>

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
        match_buy_order(order, result.trades, true);
        result.remaining_quantity = order.quantity;

        if (order.quantity > 0) {
            rest_order(order);
        }

        return result;
    }

    AddOrderResult result{.accepted = true};
    match_sell_order(order, result.trades, true);
    result.remaining_quantity = order.quantity;

    if (order.quantity > 0) {
        rest_order(order);
    }

    return result;
}

AddOrderResult OrderBook::add_market_order(Order order) {
    if (order.quantity == 0) {
        return {.accepted = false};
    }

    if (orders_by_id_.contains(order.id)) {
        return {.accepted = false};
    }

    AddOrderResult result{.accepted = true};

    if (order.side == Side::Buy) {
        match_buy_order(order, result.trades, false);
    } else {
        match_sell_order(order, result.trades, false);
    }

    result.remaining_quantity = order.quantity;
    return result;
}

void OrderBook::rest_order(Order order) {
    if (order.side == Side::Buy) {
        auto& fifo = bids_[order.price].fifo_order_ids;
        fifo.push_back(order.id);
        orders_by_id_.emplace(order.id, OrderEntry{.order = order, .fifo_position = std::prev(fifo.end())});
        return;
    }

    auto& fifo = asks_[order.price].fifo_order_ids;
    fifo.push_back(order.id);
    orders_by_id_.emplace(order.id, OrderEntry{.order = order, .fifo_position = std::prev(fifo.end())});
}

bool OrderBook::cancel_order(OrderId order_id) {
    const auto entry = orders_by_id_.find(order_id);
    if (entry == orders_by_id_.end()) {
        return false;
    }

    const Side side = entry->second.order.side;
    const Price price = entry->second.order.price;
    const auto fifo_position = entry->second.fifo_position;

    remove_from_level(side, price, fifo_position);
    orders_by_id_.erase(entry);

    return true;
}

void OrderBook::remove_from_level(Side side, Price price, std::list<OrderId>::iterator fifo_position) {
    if (side == Side::Buy) {
        auto level = bids_.find(price);
        auto& fifo = level->second.fifo_order_ids;

        fifo.erase(fifo_position);

        if (fifo.empty()) {
            bids_.erase(level);
        }

        return;
    }

    auto level = asks_.find(price);
    auto& fifo = level->second.fifo_order_ids;

    fifo.erase(fifo_position);

    if (fifo.empty()) {
        asks_.erase(level);
    }
}

void OrderBook::match_buy_order(
    Order& incoming,
    std::vector<Trade>& trades,
    bool enforce_price_limit
) {
    while (incoming.quantity > 0 && !asks_.empty()) {
        auto best_ask = asks_.begin();
        if (enforce_price_limit && best_ask->first > incoming.price) {
            break;
        }

        auto& fifo = best_ask->second.fifo_order_ids;
        const OrderId resting_id = fifo.front();
        auto resting = orders_by_id_.find(resting_id);

        const Quantity trade_quantity = std::min(incoming.quantity, resting->second.order.quantity);
        trades.push_back(
            {
                .resting_order_id = resting_id,
                .incoming_order_id = incoming.id,
                .price = resting->second.order.price,
                .quantity = trade_quantity,
            }
        );

        incoming.quantity -= trade_quantity;
        resting->second.order.quantity -= trade_quantity;

        if (resting->second.order.quantity == 0) {
            orders_by_id_.erase(resting);
            fifo.pop_front();

            if (fifo.empty()) {
                asks_.erase(best_ask);
            }
        }
    }
}

void OrderBook::match_sell_order(
    Order& incoming,
    std::vector<Trade>& trades,
    bool enforce_price_limit
) {
    while (incoming.quantity > 0 && !bids_.empty()) {
        auto best_bid = bids_.begin();
        if (enforce_price_limit && best_bid->first < incoming.price) {
            break;
        }

        auto& fifo = best_bid->second.fifo_order_ids;
        const OrderId resting_id = fifo.front();
        auto resting = orders_by_id_.find(resting_id);

        const Quantity trade_quantity = std::min(incoming.quantity, resting->second.order.quantity);
        trades.push_back(
            {
                .resting_order_id = resting_id,
                .incoming_order_id = incoming.id,
                .price = resting->second.order.price,
                .quantity = trade_quantity,
            }
        );

        incoming.quantity -= trade_quantity;
        resting->second.order.quantity -= trade_quantity;

        if (resting->second.order.quantity == 0) {
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
