#pragma once
#include <cstdint>

struct __attribute__((packed)) AddOrderMessage {
    char     message_type;
    uint16_t stock_locate;
    uint16_t tracking_number; 
    uint8_t  timestamp[6];    
    uint64_t order_ref;
    char     side;
    uint32_t shares;
    char     stock[8];
    uint32_t price;         
};

struct __attribute__((packed)) OrderCancelMessage {
    char     message_type;    
    uint16_t stock_locate;    
    uint16_t tracking_number; 
    uint8_t  timestamp[6];    
    uint64_t order_ref;       
    uint32_t cancelled_shares;
};

struct __attribute__((packed)) OrderDeleteMessage {
    char     message_type;    
    uint16_t stock_locate;    
    uint16_t tracking_number; 
    uint8_t  timestamp[6];    
    uint64_t order_ref;       
};

struct __attribute__((packed)) OrderExecutedMessage {
    char     message_type;    
    uint16_t stock_locate;    
    uint16_t tracking_number; 
    uint8_t  timestamp[6];    
    uint64_t order_ref;       
    uint32_t executed_shares; 
    uint64_t match_number;    
};