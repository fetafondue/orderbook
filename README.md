# Order book
This is my implementation of an order book.

# Coding semantics
1. Member functions are named with an underscore suffix
2. Accessors return const ref only for non primitive objects
3. Where an object needs to be referenced from more than one place, a shared pointer is used

# Time complexity
- Adding of orders: O(1)
- Cancelling orders: O(N), where N is the number of orders in the order book

# Implementation
## Level Information
For each price (level), level information will show the number of orders. Level information is aggregated into a vector of level information objects. Within this level information class, bids and asks will be two separate vectors.

## Order Book
There are two sets of orders
- The aggregated list of all orders (both bids and asks), basically every order in the entire order book
- The separated lists of bid and ask orders

### Orders
Order objects contain all the information about the order. Throughout the order book, they will be passed around as pointers because the same order will be stored in both sets of orders as mentioned above.

Requests to modify orders will be in the form of OrderModify objects.

All order entries in the order book will be stored as an object that has a pointer to the order and the iterator to the location of the order in the list of orders. The iterator will help erase the order from the list in O(1) time.

The entire order book will then be stored as an unordered map of orderId to order entry.

### Trades
When an order is filled, a trade needs to be filed, and it will be aggregated into a vector of trades. Each trade will have the orderId, price and quantity. This means that the entry for the bid will be separate from the ask that filled the bid.

### Bids
Bids within the order book is implemented as a map of price to list of orders. This map is in descending order as the highest bid is the best bid. We always will the highest price bid first, and if bids are the same price then it is on a first come first served basis.

### Asks
Likewise, asks within the order book is implemented as a map of price to list of orders. This map is in ascending order as the lowest ask is the best ask. We always will the lowest price ask first, and if asks are the same price then it is on a first come first served basis.