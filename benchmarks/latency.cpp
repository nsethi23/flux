#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <cstring>
#include "order_book.h"
#include "itch_messages.h"

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// GT server (Intel Xeon Gold 6154) base frequency
static constexpr double CPU_GHZ = 2.8;

static double cycles_to_ns(uint64_t cycles) {
    return (double)cycles / CPU_GHZ;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ./flux_latency <itch_file>\n";
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "Could not open file\n";
        return 1;
    }

    const int MAX_SAMPLES = 1'000'000;
    std::vector<uint64_t> samples;
    samples.reserve(MAX_SAMPLES);

    OrderBook book;
    char buf[128];

    while (file && (int)samples.size() < MAX_SAMPLES) {
        uint16_t msg_len = 0;
        file.read(reinterpret_cast<char*>(&msg_len), 2);
        if (!file) break;
        msg_len = __builtin_bswap16(msg_len);

        file.read(buf, msg_len);
        if (!file) break;

        char msg_type = buf[0];

        uint64_t start = rdtsc();

        switch (msg_type) {
            case 'A': {
                const AddOrderMessage* msg = reinterpret_cast<const AddOrderMessage*>(buf);
                Order o;
                o.order_id = __builtin_bswap64(msg->order_ref);
                o.price = __builtin_bswap32(msg->price);
                o.quantity = __builtin_bswap32(msg->shares);
                o.side = (msg->side == 'B') ? Side::BID : Side::ASK;
                o.timestamp = 0;
                book.add_order(o);
                break;
            }
            case 'X': {
                const OrderCancelMessage* msg = reinterpret_cast<const OrderCancelMessage*>(buf);
                book.cancel_order(__builtin_bswap64(msg->order_ref), __builtin_bswap32(msg->cancelled_shares));
                break;
            }
            case 'D': {
                const OrderDeleteMessage* msg = reinterpret_cast<const OrderDeleteMessage*>(buf);
                book.cancel_order(__builtin_bswap64(msg->order_ref), UINT64_MAX);
                break;
            }
            case 'E': {
                const OrderExecutedMessage* msg = reinterpret_cast<const OrderExecutedMessage*>(buf);
                book.execute_order(__builtin_bswap64(msg->order_ref), __builtin_bswap32(msg->executed_shares));
                break;
            }
            default:
                continue;
        }

        uint64_t end = rdtsc();
        samples.push_back(end - start);
    }

    std::sort(samples.begin(), samples.end());

    int n = samples.size();
    std::cout << "Samples collected: " << n << "\n";
    std::cout << "p50:  " << cycles_to_ns(samples[n * 0.50]) << " ns\n";
    std::cout << "p99:  " << cycles_to_ns(samples[n * 0.99]) << " ns\n";
    std::cout << "p999: " << cycles_to_ns(samples[n * 0.999]) << " ns\n";

    return 0;
}