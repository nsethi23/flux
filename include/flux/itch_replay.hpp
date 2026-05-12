#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <unordered_map>

#include "flux/itch.hpp"
#include "flux/matching_engine.hpp"

namespace flux::itch {

enum class ReplayAction {
    Added,
    Executed,
    Canceled,
    Deleted,
    Replaced,
    Ignored,
    Rejected,
    UnknownOrder,
};

struct ReplayResult {
    ReplayAction action{ReplayAction::Rejected};
};

struct ReplaySummary {
    std::size_t added{};
    std::size_t executed{};
    std::size_t canceled{};
    std::size_t deleted{};
    std::size_t replaced{};
    std::size_t ignored{};
    std::size_t rejected{};
    std::size_t unknown_orders{};
};

class ReplayHandler {
public:
    explicit ReplayHandler(MatchingEngine& engine);

    ReplayResult apply(const Message& message);
    ReplaySummary apply_all(std::span<const FeedMessage> messages);

private:
    ReplayResult apply_add_order(const AddOrder& message);
    ReplayResult apply_add_order_with_mpid(const AddOrderWithMpid& message);
    ReplayResult apply_order_executed(const OrderExecuted& message);
    ReplayResult apply_order_executed_with_price(const OrderExecutedWithPrice& message);
    ReplayResult apply_order_cancel(const OrderCancel& message);
    ReplayResult apply_order_delete(const OrderDelete& message);
    ReplayResult apply_order_replace(const OrderReplace& message);
    ReplayResult apply_stock_directory(const StockDirectory& message);

    MatchingEngine& engine_;
    std::unordered_map<OrderId, std::string> symbol_by_order_id_;
};

}  // namespace flux::itch
