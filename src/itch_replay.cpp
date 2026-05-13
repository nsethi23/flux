#include "flux/itch_replay.hpp"

#include <type_traits>
#include <variant>

namespace flux::itch {

ReplayHandler::ReplayHandler(MatchingEngine& engine) : engine_(engine) {}

ReplayResult ReplayHandler::apply(const Message& message) {
    return std::visit(
        [this](const auto& typed_message) {
            using MessageType = std::decay_t<decltype(typed_message)>;

            if constexpr (std::is_same_v<MessageType, AddOrder>) {
                return this->apply_add_order(typed_message);
            } else if constexpr (std::is_same_v<MessageType, AddOrderWithMpid>) {
                return this->apply_add_order_with_mpid(typed_message);
            } else if constexpr (std::is_same_v<MessageType, OrderExecuted>) {
                return this->apply_order_executed(typed_message);
            } else if constexpr (std::is_same_v<MessageType, OrderExecutedWithPrice>) {
                return this->apply_order_executed_with_price(typed_message);
            } else if constexpr (std::is_same_v<MessageType, OrderCancel>) {
                return this->apply_order_cancel(typed_message);
            } else if constexpr (std::is_same_v<MessageType, OrderDelete>) {
                return this->apply_order_delete(typed_message);
            } else if constexpr (std::is_same_v<MessageType, OrderReplace>) {
                return this->apply_order_replace(typed_message);
            } else {
                return this->apply_stock_directory(typed_message);
            }
        },
        message
    );
}

ReplaySummary ReplayHandler::apply_all(std::span<const FeedMessage> messages) {
    ReplaySummary summary;

    for (const auto& feed_message : messages) {
        const auto result = apply(feed_message.message);

        switch (result.action) {
            case ReplayAction::Added:
                ++summary.added;
                break;
            case ReplayAction::Executed:
                ++summary.executed;
                break;
            case ReplayAction::Canceled:
                ++summary.canceled;
                break;
            case ReplayAction::Deleted:
                ++summary.deleted;
                break;
            case ReplayAction::Replaced:
                ++summary.replaced;
                break;
            case ReplayAction::Ignored:
                ++summary.ignored;
                break;
            case ReplayAction::Rejected:
                ++summary.rejected;
                break;
            case ReplayAction::UnknownOrder:
                ++summary.unknown_orders;
                break;
        }
    }

    return summary;
}

ReplayResult ReplayHandler::apply_add_order(const AddOrder& message) {
    auto& book = engine_.book_for(message.stock);
    const auto result = book.add_limit_order(
        {
            .id = message.order_id,
            .side = message.side,
            .price = message.price,
            .quantity = message.quantity,
        }
    );

    if (!result.accepted) {
        return {.action = ReplayAction::Rejected};
    }

    if (result.remaining_quantity > 0) {
        symbol_by_order_id_[message.order_id] = message.stock;
    }

    return {.action = ReplayAction::Added};
}

ReplayResult ReplayHandler::apply_add_order_with_mpid(const AddOrderWithMpid& message) {
    return apply_add_order(
        {
            .header = message.header,
            .order_id = message.order_id,
            .side = message.side,
            .quantity = message.quantity,
            .stock = message.stock,
            .price = message.price,
        }
    );
}

ReplayResult ReplayHandler::apply_order_executed(const OrderExecuted& message) {
    const auto symbol = symbol_by_order_id_.find(message.order_id);
    if (symbol == symbol_by_order_id_.end()) {
        return {.action = ReplayAction::UnknownOrder};
    }

    auto& book = engine_.book_for(symbol->second);
    if (!book.reduce_order_quantity(message.order_id, message.executed_quantity)) {
        symbol_by_order_id_.erase(symbol);
        return {.action = ReplayAction::UnknownOrder};
    }

    if (!book.order_status(message.order_id).has_value()) {
        symbol_by_order_id_.erase(symbol);
    }

    return {.action = ReplayAction::Executed};
}

ReplayResult ReplayHandler::apply_order_executed_with_price(const OrderExecutedWithPrice& message) {
    return apply_order_executed(
        {
            .header = message.header,
            .order_id = message.order_id,
            .executed_quantity = message.executed_quantity,
            .match_number = message.match_number,
        }
    );
}

ReplayResult ReplayHandler::apply_order_cancel(const OrderCancel& message) {
    const auto symbol = symbol_by_order_id_.find(message.order_id);
    if (symbol == symbol_by_order_id_.end()) {
        return {.action = ReplayAction::UnknownOrder};
    }

    auto& book = engine_.book_for(symbol->second);
    if (!book.reduce_order_quantity(message.order_id, message.canceled_quantity)) {
        symbol_by_order_id_.erase(symbol);
        return {.action = ReplayAction::UnknownOrder};
    }

    if (!book.order_status(message.order_id).has_value()) {
        symbol_by_order_id_.erase(symbol);
    }

    return {.action = ReplayAction::Canceled};
}

ReplayResult ReplayHandler::apply_order_delete(const OrderDelete& message) {
    const auto symbol = symbol_by_order_id_.find(message.order_id);
    if (symbol == symbol_by_order_id_.end()) {
        return {.action = ReplayAction::UnknownOrder};
    }

    auto& book = engine_.book_for(symbol->second);
    if (!book.cancel_order(message.order_id)) {
        symbol_by_order_id_.erase(symbol);
        return {.action = ReplayAction::UnknownOrder};
    }

    symbol_by_order_id_.erase(symbol);
    return {.action = ReplayAction::Deleted};
}

ReplayResult ReplayHandler::apply_order_replace(const OrderReplace& message) {
    const auto symbol = symbol_by_order_id_.find(message.original_order_id);
    if (symbol == symbol_by_order_id_.end()) {
        return {.action = ReplayAction::UnknownOrder};
    }

    auto& book = engine_.book_for(symbol->second);
    const auto status = book.order_status(message.original_order_id);
    if (!status.has_value()) {
        symbol_by_order_id_.erase(symbol);
        return {.action = ReplayAction::UnknownOrder};
    }

    const std::string stock = symbol->second;
    const auto replaced = book.replace_order(
        message.original_order_id,
        {
            .id = message.new_order_id,
            .side = status->side,
            .price = message.price,
            .quantity = message.quantity,
        }
    );

    if (!replaced.accepted) {
        return {.action = ReplayAction::Rejected};
    }

    symbol_by_order_id_.erase(symbol);
    symbol_by_order_id_[message.new_order_id] = stock;
    return {.action = ReplayAction::Replaced};
}

ReplayResult ReplayHandler::apply_stock_directory(const StockDirectory& /*message*/) {
    return {.action = ReplayAction::Ignored};
}

}  // namespace flux::itch
