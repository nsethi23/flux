# Flux Architecture

Flux is organized around a reusable `flux_core` library.

## Core Types

- `OrderId`: unsigned 64-bit order identifier.
- `Price`: signed 64-bit integer price. Prices are integers to avoid floating-point rounding.
- `Quantity`: unsigned 32-bit share quantity.
- `Side`: buy or sell.

## Order Book

`OrderBook` represents one instrument.

Data structures:

- `std::map<Price, PriceLevel, std::greater<Price>> bids_`
  - highest bid appears at `begin()`.
- `std::map<Price, PriceLevel, std::less<Price>> asks_`
  - lowest ask appears at `begin()`.
- `std::list<OrderId>` inside each price level
  - preserves FIFO priority and gives stable iterators.
- `std::unordered_map<OrderId, OrderEntry>`
  - direct order lookup by ID.

Each `OrderEntry` stores both the order and its iterator inside the price-level FIFO list. That makes cancellation O(1) after the ID lookup.

## Matching Rules

Limit buy:

- matches lowest asks while `buy_price >= best_ask`.
- rests remaining quantity as a bid.

Limit sell:

- matches highest bids while `sell_price <= best_bid`.
- rests remaining quantity as an ask.

Market buy:

- consumes asks from lowest price upward.
- never rests unfilled quantity.

Market sell:

- consumes bids from highest price downward.
- never rests unfilled quantity.

Trades execute at the resting order's price.

## Multi-Symbol Engine

`MatchingEngine` maps symbols to independent `OrderBook` instances. This keeps per-symbol price-time priority isolated.

## ITCH Parser

The ITCH parser currently supports a focused subset:

- `A`: Add Order
- `E`: Order Executed
- `X`: Order Cancel
- `D`: Order Delete

All integer fields are decoded as big-endian values. Timestamps are 6-byte integers.
