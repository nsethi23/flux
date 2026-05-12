#pragma once

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
    Rejected,
    UnknownOrder,
};

struct ReplayResult {
    ReplayAction action{ReplayAction::Rejected};
};

class ReplayHandler {
public:
    explicit ReplayHandler(MatchingEngine& engine);

    ReplayResult apply(const Message& message);

private:
    ReplayResult apply_add_order(const AddOrder& message);
    ReplayResult apply_order_executed(const OrderExecuted& message);
    ReplayResult apply_order_cancel(const OrderCancel& message);
    ReplayResult apply_order_delete(const OrderDelete& message);

    MatchingEngine& engine_;
    std::unordered_map<OrderId, std::string> symbol_by_order_id_;
};

}  // namespace flux::itch
