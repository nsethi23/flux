#include "flux/order_book.hpp"

#include <algorithm>
#include <functional>
#include <iterator>

namespace flux {

OrderBook::OrderBook(std::size_t order_capacity_hint) {
    if (order_capacity_hint > 0) {
        orders_by_id_.reserve(order_capacity_hint);
    }
}

void OrderBook::set_listener(BookListener* listener) {
    listener_ = listener;
}

AddOrderResult OrderBook::add_limit_order(Order order) {
    if (order.quantity == 0) {
        return {.accepted = false, .reject_reason = AddOrderRejectReason::ZeroQuantity};
    }

    if (orders_by_id_.contains(order.id)) {
        return {.accepted = false, .reject_reason = AddOrderRejectReason::DuplicateOrderId};
    }

    if (order.time_in_force == TimeInForce::FillOrKill && !can_fully_fill(order)) {
        return {.accepted = false, .reject_reason = AddOrderRejectReason::FillOrKillNotFilled};
    }

    const TopOfBook before = listener_ != nullptr ? top_of_book() : TopOfBook{};

    if (order.side == Side::Buy) {
        AddOrderResult result{.accepted = true};
        match_buy_order(order, result.trades, true);
        result.remaining_quantity = order.quantity;

        if (order.quantity > 0 && order.time_in_force == TimeInForce::GoodTillCancel) {
            rest_order(order);
        }

        notify_trades(result.trades);
        notify_top_if_changed(before);
        return result;
    }

    AddOrderResult result{.accepted = true};
    match_sell_order(order, result.trades, true);
    result.remaining_quantity = order.quantity;

    if (order.quantity > 0 && order.time_in_force == TimeInForce::GoodTillCancel) {
        rest_order(order);
    }

    notify_trades(result.trades);
    notify_top_if_changed(before);
    return result;
}

AddOrderResult OrderBook::add_market_order(Order order) {
    if (order.quantity == 0) {
        return {.accepted = false, .reject_reason = AddOrderRejectReason::ZeroQuantity};
    }

    if (orders_by_id_.contains(order.id)) {
        return {.accepted = false, .reject_reason = AddOrderRejectReason::DuplicateOrderId};
    }

    const TopOfBook before = listener_ != nullptr ? top_of_book() : TopOfBook{};
    AddOrderResult result{.accepted = true};

    if (order.side == Side::Buy) {
        match_buy_order(order, result.trades, false);
    } else {
        match_sell_order(order, result.trades, false);
    }

    result.remaining_quantity = order.quantity;
    notify_trades(result.trades);
    notify_top_if_changed(before);
    return result;
}

AddOrderResult OrderBook::add_resting_order(Order order) {
    if (order.quantity == 0) {
        return {.accepted = false, .reject_reason = AddOrderRejectReason::ZeroQuantity};
    }

    if (orders_by_id_.contains(order.id)) {
        return {.accepted = false, .reject_reason = AddOrderRejectReason::DuplicateOrderId};
    }

    const TopOfBook before = listener_ != nullptr ? top_of_book() : TopOfBook{};
    rest_order(order);
    notify_top_if_changed(before);

    return {.accepted = true, .remaining_quantity = order.quantity};
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

    const TopOfBook before = listener_ != nullptr ? top_of_book() : TopOfBook{};
    const Side side = entry->second.order.side;
    const Price price = entry->second.order.price;
    const auto fifo_position = entry->second.fifo_position;

    remove_from_level(side, price, fifo_position);
    orders_by_id_.erase(entry);

    notify_top_if_changed(before);
    return true;
}

AddOrderResult OrderBook::replace_order(OrderId existing_order_id, Order replacement) {
    if (replacement.quantity == 0) {
        return {.accepted = false, .reject_reason = AddOrderRejectReason::ZeroQuantity};
    }

    if (replacement.id != existing_order_id && orders_by_id_.contains(replacement.id)) {
        return {.accepted = false, .reject_reason = AddOrderRejectReason::DuplicateOrderId};
    }

    if (!cancel_order(existing_order_id)) {
        return {.accepted = false};
    }

    return add_limit_order(replacement);
}

bool OrderBook::reduce_order_quantity(OrderId order_id, Quantity quantity_to_reduce) {
    if (quantity_to_reduce == 0) {
        return false;
    }

    auto entry = orders_by_id_.find(order_id);
    if (entry == orders_by_id_.end()) {
        return false;
    }

    const TopOfBook before = listener_ != nullptr ? top_of_book() : TopOfBook{};

    if (quantity_to_reduce >= entry->second.order.quantity) {
        const Side side = entry->second.order.side;
        const Price price = entry->second.order.price;
        const auto fifo_position = entry->second.fifo_position;

        remove_from_level(side, price, fifo_position);
        orders_by_id_.erase(entry);
        notify_top_if_changed(before);
        return true;
    }

    entry->second.order.quantity -= quantity_to_reduce;
    notify_top_if_changed(before);
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

std::optional<OrderStatus> OrderBook::order_status(OrderId order_id) const {
    const auto entry = orders_by_id_.find(order_id);
    if (entry == orders_by_id_.end()) {
        return std::nullopt;
    }

    return OrderStatus{
        .side = entry->second.order.side,
        .price = entry->second.order.price,
        .quantity = entry->second.order.quantity,
    };
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

bool OrderBook::can_fully_fill(const Order& order) const {
    Quantity remaining = order.quantity;

    if (order.side == Side::Buy) {
        for (const auto& [price, level] : asks_) {
            if (price > order.price) {
                break;
            }

            for (const OrderId order_id : level.fifo_order_ids) {
                const auto resting = orders_by_id_.find(order_id);
                remaining -= std::min(remaining, resting->second.order.quantity);
                if (remaining == 0) {
                    return true;
                }
            }
        }

        return false;
    }

    for (const auto& [price, level] : bids_) {
        if (price < order.price) {
            break;
        }

        for (const OrderId order_id : level.fifo_order_ids) {
            const auto resting = orders_by_id_.find(order_id);
            remaining -= std::min(remaining, resting->second.order.quantity);
            if (remaining == 0) {
                return true;
            }
        }
    }

    return false;
}

TopOfBook OrderBook::top_of_book() const {
    TopOfBook top{.bid = best_bid(), .ask = best_ask()};

    if (!bids_.empty()) {
        for (const OrderId order_id : bids_.begin()->second.fifo_order_ids) {
            const auto resting = orders_by_id_.find(order_id);
            top.bid_quantity += resting->second.order.quantity;
        }
    }

    if (!asks_.empty()) {
        for (const OrderId order_id : asks_.begin()->second.fifo_order_ids) {
            const auto resting = orders_by_id_.find(order_id);
            top.ask_quantity += resting->second.order.quantity;
        }
    }

    return top;
}

void OrderBook::notify_trades(const std::vector<Trade>& trades) {
    if (listener_ == nullptr) {
        return;
    }

    for (const auto& trade : trades) {
        listener_->on_trade(trade);
    }
}

void OrderBook::notify_top_if_changed(const TopOfBook& before) {
    if (listener_ == nullptr) {
        return;
    }

    const TopOfBook after = top_of_book();
    if (before.bid != after.bid || before.ask != after.ask ||
        before.bid_quantity != after.bid_quantity || before.ask_quantity != after.ask_quantity) {
        listener_->on_top_of_book_change(after);
    }
}

}  // namespace flux
