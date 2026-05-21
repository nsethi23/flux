#include "order_book.h"

void OrderBook::add_order(const Order& order) {
    if (order.side == Side::BID) {
        auto it = bids.find(order.price);
        if (it == bids.end()) {
            bids.emplace(order.price, PriceLevel(order.price));
            it = bids.find(order.price);
        }

        it -> second.orders.push_back(order);
        it -> second.total_quantity += order.quantity;
    } else {
        auto it = asks.find(order.price);
        if (it == asks.end()) {
            asks.emplace(order.price, PriceLevel(order.price));
            it = asks.find(order.price);
        }

        it -> second.orders.push_back(order);
        it -> second.total_quantity += order.quantity;
    }
}

void OrderBook::cancel_order(uint64_t order_id, uint64_t quantity) {
    for (auto& [price, level] : bids) {
        for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
            if (it -> order_id == order_id) {
                it -> quantity  -= quantity;
                level.total_quantity -= quantity;
                if (it -> quantity == 0) {
                    level.orders.erase(it);
                }
                if (level.orders.empty()) {
                    bids.erase(price);
                }
                return;
            }
        }
    }
    for (auto& [price, level] : asks) {
        for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
            if (it -> order_id == order_id) {
                it -> quantity  -= quantity;
                level.total_quantity -= quantity;
                if (it -> quantity == 0) {
                    level.orders.erase(it);
                }
                if (level.orders.empty()) {
                    asks.erase(price);
                }
                return;
            }
        }
    }
}

void OrderBook::execute_order(uint64_t order_id, uint64_t quantity) {
    for (auto& [price, level] : bids) {
        for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
            if (it -> order_id == order_id) {
                it -> quantity  -= quantity;
                level.total_quantity -= quantity;
                if (it -> quantity == 0) {
                    level.orders.erase(it);
                }
                if (level.orders.empty()) {
                    bids.erase(price);
                }
                return;
            }
        }
    }
    for (auto& [price, level] : asks) {
        for (auto it = level.orders.begin(); it != level.orders.end(); ++it) {
            if (it -> order_id == order_id) {
                it -> quantity  -= quantity;
                level.total_quantity -= quantity;
                if (it -> quantity == 0) {
                    level.orders.erase(it);
                }
                if (level.orders.empty()) {
                    asks.erase(price);
                }
                return;
            }
        }
    }
}