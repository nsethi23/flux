#include "flux/matching_engine.hpp"

namespace flux {

OrderBook& MatchingEngine::book_for(std::string_view symbol) {
    return books_by_symbol_[std::string{symbol}];
}

const OrderBook* MatchingEngine::find_book(std::string_view symbol) const {
    const auto book = books_by_symbol_.find(std::string{symbol});
    if (book == books_by_symbol_.end()) {
        return nullptr;
    }

    return &book->second;
}

std::size_t MatchingEngine::symbol_count() const {
    return books_by_symbol_.size();
}

}  // namespace flux
