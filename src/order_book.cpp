#include "order_book.h"
#include <algorithm>

void OrderBook::add_order(Order order) {
    if (order.side == Side::BID) {
        while (order.quantity > 0 && !asks.empty()) {
            auto& best_ask = asks.begin()->second;
            if (order.price < best_ask.price) break;
            auto& resting_order = *best_ask.orders.front();
            uint64_t fill_qty = std::min(order.quantity, resting_order.quantity);
            order.quantity -= fill_qty;
            resting_order.quantity -= fill_qty;
            best_ask.total_quantity -= fill_qty;
            if (resting_order.quantity == 0) {
                pool_.deallocate(best_ask.orders.front());
                best_ask.orders.pop_front();
            }
            if (best_ask.orders.empty()) asks.erase(asks.begin());
        }
    } else {
        while (order.quantity > 0 && !bids.empty()) {
            auto& best_bid = bids.begin()->second;
            if (order.price > best_bid.price) break;
            auto& resting_order = *best_bid.orders.front();
            uint64_t fill_qty = std::min(order.quantity, resting_order.quantity);
            order.quantity -= fill_qty;
            resting_order.quantity -= fill_qty;
            best_bid.total_quantity -= fill_qty;
            if (resting_order.quantity == 0) {
                pool_.deallocate(best_bid.orders.front());
                best_bid.orders.pop_front();
            }
            if (best_bid.orders.empty()) bids.erase(bids.begin());
        }
    }

    if (order.quantity > 0) {
        if (order.side == Side::BID) {
            auto it = bids.find(order.price);
            if (it == bids.end()) {
                bids.emplace(order.price, PriceLevel(order.price));
                it = bids.find(order.price);
            }
            Order* pooled = pool_.allocate();
            *pooled = order;
            it->second.orders.push_back(pooled);
            order_map_[order.order_id] = {order.side, order.price};
            it->second.total_quantity += order.quantity;
        } else {
            auto it = asks.find(order.price);
            if (it == asks.end()) {
                asks.emplace(order.price, PriceLevel(order.price));
                it = asks.find(order.price);
            }
            Order* pooled = pool_.allocate();
            *pooled = order;
            it->second.orders.push_back(pooled);
            order_map_[order.order_id] = {order.side, order.price};
            it->second.total_quantity += order.quantity;
        }
    }
}

void OrderBook::cancel_order(uint64_t order_id, uint64_t quantity) {
    auto map_it = order_map_.find(order_id);
    if (map_it == order_map_.end()) return;

    OrderLocation loc = map_it->second;

    if (loc.side == Side::BID) {
        auto level_it = bids.find(loc.price);
        if (level_it == bids.end()) return;
        PriceLevel& level = level_it->second;
        for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
            if ((*it)->order_id == order_id) {
                if (quantity == UINT64_MAX || quantity >= (*it)->quantity) {
                    level.total_quantity -= (*it)->quantity;
                    pool_.deallocate(*it);
                    level.orders.erase(it);
                    if (level.orders.empty()) bids.erase(level_it);
                    order_map_.erase(map_it);
                } else {
                    (*it)->quantity -= quantity;
                    level.total_quantity -= quantity;
                }
                return;
            }
        }
    } else {
        auto level_it = asks.find(loc.price);
        if (level_it == asks.end()) return;
        PriceLevel& level = level_it->second;
        for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
            if ((*it)->order_id == order_id) {
                if (quantity == UINT64_MAX || quantity >= (*it)->quantity) {
                    level.total_quantity -= (*it)->quantity;
                    pool_.deallocate(*it);
                    level.orders.erase(it);
                    if (level.orders.empty()) asks.erase(level_it);
                    order_map_.erase(map_it);
                } else {
                    (*it)->quantity -= quantity;
                    level.total_quantity -= quantity;
                }
                return;
            }
        }
    }
}

void OrderBook::execute_order(uint64_t order_id, uint64_t quantity) {
    auto map_it = order_map_.find(order_id);
    if (map_it == order_map_.end()) return;

    OrderLocation loc = map_it->second;

    if (loc.side == Side::BID) {
        auto level_it = bids.find(loc.price);
        if (level_it == bids.end()) return;
        PriceLevel& level = level_it->second;
        for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
            if ((*it)->order_id == order_id) {
                if (quantity >= (*it)->quantity) {
                    level.total_quantity -= (*it)->quantity;
                    pool_.deallocate(*it);
                    level.orders.erase(it);
                    if (level.orders.empty()) bids.erase(level_it);
                    order_map_.erase(map_it);
                } else {
                    (*it)->quantity -= quantity;
                    level.total_quantity -= quantity;
                }
                return;
            }
        }
    } else {
        auto level_it = asks.find(loc.price);
        if (level_it == asks.end()) return;
        PriceLevel& level = level_it->second;
        for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
            if ((*it)->order_id == order_id) {
                if (quantity >= (*it)->quantity) {
                    level.total_quantity -= (*it)->quantity;
                    pool_.deallocate(*it);
                    level.orders.erase(it);
                    if (level.orders.empty()) asks.erase(level_it);
                    order_map_.erase(map_it);
                } else {
                    (*it)->quantity -= quantity;
                    level.total_quantity -= quantity;
                }
                return;
            }
        }
    }
}

int64_t OrderBook::best_bid() const {
    if (bids.empty()) return -1;
    return bids.begin()->first;
}

int64_t OrderBook::best_ask() const {
    if (asks.empty()) return -1;
    return asks.begin()->first;
}

uint64_t OrderBook::best_bid_quantity() const {
    if (bids.empty()) return 0;
    return bids.begin()->second.total_quantity;
}

uint64_t OrderBook::best_ask_quantity() const {
    if (asks.empty()) return 0;
    return asks.begin()->second.total_quantity;
}