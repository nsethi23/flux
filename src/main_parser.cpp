#include <iostream>
#include "order_book.h"
#include "itch_parser.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./flux_parser <itch_file>\n";
        return 1;
    }

    OrderBook book;
    ITCHParser parser(book);
    parser.parse_file(argv[1]);

    std::cout << "Best bid: " << book.best_bid() << "\n";
    std::cout << "Best ask: " << book.best_ask() << "\n";

    return 0;
}