#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include <map>
#include <unordered_map>

#include "Trade.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderBookLevelInfos.h"

class OrderBook
{
public:
    
    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades MatchOrder(OrderModify order);
    
    std::size_t Size() const;
    OrderBookLevelInfos GetOrderInfos() const;
    
private:
    struct OrderEntry
    {
        OrderPointer order_ { nullptr };
        OrderPointers::iterator location_;
    };
    
    // descending order from best to worst bid
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    // ascending order from best to worst ask
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;
    
    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();
};


#endif