#include "itch_parser.h"
#include <fstream>
#include <iostream>
#include <cstring>

ITCHParser::ITCHParser(OrderBook& book) : book_(book) {}

void ITCHParser::parse_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Could not open file: " << filename << "\n";
        return;
    }

    char buf[128];
    uint64_t msg_count = 0;

    while (file) {
        uint16_t msg_len = 0;
        file.read(reinterpret_cast<char*>(&msg_len), 2);
        if (!file) break;
        msg_len = __builtin_bswap16(msg_len);

        file.read(buf, msg_len);
        if (!file) break;

        char msg_type = buf[0];
        switch (msg_type) {
            case 'A': handle_add_order(buf);    break;
            case 'X': handle_cancel_order(buf); break;
            case 'D': handle_delete_order(buf); break;
            case 'E': handle_execute_order(buf); break;
            default: break; // ignore other message types
        }

        ++msg_count;
        if (msg_count % 1'000'000 == 0) {
            std::cout << "Processed " << msg_count << " messages\n";
        }
    }

    std::cout << "Total messages processed: " << msg_count << "\n";
}

void ITCHParser::handle_add_order(const char* buf) {
    const AddOrderMessage* msg = reinterpret_cast<const AddOrderMessage*>(buf);
    Order o;
    o.order_id = __builtin_bswap64(msg->order_ref);
    o.price    = __builtin_bswap32(msg->price);
    o.quantity = __builtin_bswap32(msg->shares);
    o.side     = (msg->side == 'B') ? Side::BID : Side::ASK;
    o.timestamp = 0;
    book_.add_order(o);
}

void ITCHParser::handle_cancel_order(const char* buf) {
    const OrderCancelMessage* msg = reinterpret_cast<const OrderCancelMessage*>(buf);
    book_.cancel_order(
        __builtin_bswap64(msg->order_ref),
        __builtin_bswap32(msg->cancelled_shares)
    );
}

void ITCHParser::handle_delete_order(const char* buf) {
    const OrderDeleteMessage* msg = reinterpret_cast<const OrderDeleteMessage*>(buf);
    book_.cancel_order(
        __builtin_bswap64(msg->order_ref),
        UINT64_MAX // signal full delete
    );
}

void ITCHParser::handle_execute_order(const char* buf) {
    const OrderExecutedMessage* msg = reinterpret_cast<const OrderExecutedMessage*>(buf);
    book_.execute_order(
        __builtin_bswap64(msg->order_ref),
        __builtin_bswap32(msg->executed_shares)
    );
}