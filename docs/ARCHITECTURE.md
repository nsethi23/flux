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

The `std::list` choice is a deliberate correctness/performance tradeoff. It gives stable iterators and simple O(1) arbitrary cancel, but it has poor cache locality and per-node allocation. A production-oriented next step would replace it with a pool-backed intrusive queue.

## Events

`OrderBook` can publish events through `BookListener`:

- `on_trade`
- `on_top_of_book_change`

Top-of-book events include best bid/ask prices and aggregate quantity at those best levels.

## Matching Rules

Orders support three time-in-force policies:

- `GoodTillCancel`: match immediately if marketable, then rest any remaining quantity.
- `ImmediateOrCancel`: match immediately, then cancel any remaining quantity.
- `FillOrKill`: execute only if the full quantity can fill immediately; otherwise reject without mutating the book.

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
- `F`: Add Order with MPID Attribution
- `E`: Order Executed
- `C`: Order Executed With Price
- `X`: Order Cancel
- `D`: Order Delete
- `U`: Order Replace
- `R`: Stock Directory

All integer fields are decoded as big-endian values. Timestamps are 6-byte integers.

`parse_message` parses one raw ITCH message payload. `parse_feed` parses a memory buffer containing length-prefixed messages. Each frame is:

```text
2-byte big-endian message length
N-byte ITCH message payload
```

The streaming `parse_feed` overload invokes a callback for each supported message instead of materializing the whole feed as a vector. Unknown message types are skipped and counted so real ITCH files with unsupported administrative messages can still replay.

## ITCH Replay

`itch::ReplayHandler` applies parsed ITCH messages to a `MatchingEngine`.

Mapping:

- `A` Add Order -> rest order directly on the symbol's book.
- `F` Add Order with MPID Attribution -> rest order directly on the symbol's book.
- `E` Order Executed -> reduce the resting order by executed quantity.
- `C` Order Executed With Price -> reduce the resting order by executed quantity.
- `X` Order Cancel -> reduce the resting order by canceled quantity.
- `D` Order Delete -> cancel the resting order entirely.
- `U` Order Replace -> replace order ID, price, and quantity while preserving side and symbol.
- `R` Stock Directory -> parsed and counted as ignored by replay.

Execution, cancel, and delete messages identify orders by ID but do not carry the stock symbol. The replay handler therefore keeps an `order_id -> symbol` map after Add Order messages.

Replay mode is separate from simulated matching mode. In replay mode, the historical feed is the source of truth for executions; Add Order messages rest directly and do not run local matching. This avoids phantom trades when replaying a real exchange feed.
