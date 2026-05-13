#pragma once

#include <list>
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

struct TopOfBook {
    std::optional<Price> bid;
    std::optional<Price> ask;
    Quantity bid_quantity{};
    Quantity ask_quantity{};
};

class BookListener {
public:
    virtual ~BookListener() = default;

    virtual void on_trade(const Trade& trade) = 0;
    virtual void on_top_of_book_change(const TopOfBook& top_of_book) = 0;
};

enum class AddOrderRejectReason {
    None,
    ZeroQuantity,
    DuplicateOrderId,
    FillOrKillNotFilled,
};

struct AddOrderResult {
    bool accepted{};
    AddOrderRejectReason reject_reason{AddOrderRejectReason::None};
    Quantity remaining_quantity{};
    std::vector<Trade> trades;
};

struct OrderStatus {
    Side side{};
    Price price{};
    Quantity quantity{};
};

class OrderBook {
public:
    explicit OrderBook(std::size_t order_capacity_hint = 0);

    void set_listener(BookListener* listener);

    AddOrderResult add_limit_order(Order order);
    AddOrderResult add_market_order(Order order);
    AddOrderResult add_resting_order(Order order);
    bool cancel_order(OrderId order_id);
    AddOrderResult replace_order(OrderId existing_order_id, Order replacement);
    bool reduce_order_quantity(OrderId order_id, Quantity quantity_to_reduce);

    [[nodiscard]] std::optional<Price> best_bid() const;
    [[nodiscard]] std::optional<Price> best_ask() const;
    [[nodiscard]] std::optional<OrderStatus> order_status(OrderId order_id) const;
    [[nodiscard]] std::vector<OrderId> order_ids_at_price(Side side, Price price) const;
    [[nodiscard]] std::size_t order_count() const;

private:
    struct PriceLevel {
        std::list<OrderId> fifo_order_ids;
    };

    struct OrderEntry {
        Order order;
        std::list<OrderId>::iterator fifo_position;
    };

    void rest_order(Order order);
    void match_buy_order(Order& incoming, std::vector<Trade>& trades, bool enforce_price_limit);
    void match_sell_order(Order& incoming, std::vector<Trade>& trades, bool enforce_price_limit);
    void remove_from_level(Side side, Price price, std::list<OrderId>::iterator fifo_position);
    [[nodiscard]] bool can_fully_fill(const Order& order) const;
    [[nodiscard]] TopOfBook top_of_book() const;
    void notify_trades(const std::vector<Trade>& trades);
    void notify_top_if_changed(const TopOfBook& before);

    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    std::map<Price, PriceLevel, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_by_id_;
    BookListener* listener_{};
};

}  // namespace flux
