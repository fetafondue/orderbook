#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include <map>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "Trade.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderBookLevelInfos.h"

class OrderBook
{
public:
    
    Trades AddOrder(OrderPointer order);
    Trades MatchOrder(OrderModify order);
    void CancelOrder(OrderId orderId);
    
    std::size_t Size() const;
    OrderBookLevelInfos GetOrderInfos() const;
    
private:
    struct OrderEntry
    {
        OrderPointer order_ { nullptr };
        OrderPointers::iterator location_;
    };
    
    struct LevelData
    {
        Quantity quantity_{};
        Quantity count_{};
        
        enum class Action
        {
            Add,
            Remove,
            Match,
        };
    };
    
    std::unordered_map<Price, LevelData> metadata_;
    // descending order from best to worst bid
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    // ascending order from best to worst ask
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;
    mutable std::mutex ordersMutex_;
    std::thread ordersPruneThread;
    std::condition_variable shutdownConditionVariable_;
    std::atomic<bool> shutdown_ { false };
    
    void PruneGoodForDayOrders();
    
    // wrapper function to obtain mutex before loop
    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternal(OrderId orderId);
    
    void OnOrderCancelled(OrderPointer order);
    void OnOrderAdded(OrderPointer order);
    void OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled);
    void UpdateLevelData(Price price, Quantity quantity, LevelData::Action action);
    
    // used for FillOrKill orders, checks if the quantity between the order price and the best ask/bid is enough to fully fill the order
    bool CanFullyFill(Side side, Price price, Quantity quantity) const;
    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();
};


#endif