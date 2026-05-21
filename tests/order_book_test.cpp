#include <gtest/gtest.h>
#include "order_book.h"
Order make_order(uint64_t id, int64_t price, uint64_t qty, Side side)
{
    Order o;
    o.order_id = id;
    o.price = price;
    o.quantity = qty;
    o.side = side;
    o.timestamp = 0;
    return o;
}
// Add a bid and verify it's in the book
TEST(OrderBookTest, AddBidOrder)
{
    OrderBook book;
    book.add_order(make_order(1, 100, 10, Side::BID));
    EXPECT_EQ(book.best_bid(), 100);
    EXPECT_EQ(book.best_bid_quantity(), 10);
}
// Add an ask and verify it's in the book
TEST(OrderBookTest, AddAskOrder)
{
    OrderBook book;
    book.add_order(make_order(1, 100, 10, Side::ASK));
    EXPECT_EQ(book.best_ask(), 100);
    EXPECT_EQ(book.best_ask_quantity(), 10);
}
// Bid and ask at same price should match and clear
TEST(OrderBookTest, FullMatch)
{
    OrderBook book;
    book.add_order(make_order(1, 100, 10, Side::ASK));
    book.add_order(make_order(2, 100, 10, Side::BID));
    EXPECT_EQ(book.best_bid(), -1);
    EXPECT_EQ(book.best_ask(), -1);
}
// Bid larger than ask — partial fill, remainder stays in book
TEST(OrderBookTest, PartialMatch)
{
    OrderBook book;
    book.add_order(make_order(1, 100, 5, Side::ASK));
    book.add_order(make_order(2, 100, 10, Side::BID));
    EXPECT_EQ(book.best_bid(), 100);
    EXPECT_EQ(book.best_bid_quantity(), 5);
    EXPECT_EQ(book.best_ask(), -1);
}
// Cancel reduces quantity
TEST(OrderBookTest, CancelOrder)
{
    OrderBook book;
    book.add_order(make_order(1, 100, 10, Side::BID));
    book.cancel_order(1, 5);
    EXPECT_EQ(book.best_bid_quantity(), 5);
}
// Cancel fully removes order
TEST(OrderBookTest, CancelFullOrder)
{
    OrderBook book;
    book.add_order(make_order(1, 100, 10, Side::BID));
    book.cancel_order(1, 10);
    EXPECT_EQ(book.best_bid(), -1);
}
