#pragma once

#include "order_book.h"
#include "itch_messages.h"
#include <string>

class ITCHParser {
public:
    ITCHParser(OrderBook& book);
    void parse_file(const std::string& filename);

private:
    OrderBook& book_;
    void handle_add_order(const char* buf);
    void handle_cancel_order(const char* buf);
    void handle_delete_order(const char* buf);
    void handle_execute_order(const char* buf);
};