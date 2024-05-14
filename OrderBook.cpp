#include <numeric>

#include "OrderBook.h"

void OrderBook::PruneGoodForDayOrders()
{
    using namespace std::chrono;
    const auto end = hours(16);
    
    while (true)
    {
        const auto now = system_clock::now();
        const auto now_c = system_clock::to_time_t(now);
        std::tm now_parts;
        localtime_r(&now_c, &now_parts);
        
        if (now_parts.tm_hour >= end.count())
            now_parts.tm_mday += 1;
        
        now_parts.tm_hour = end.count();
        now_parts.tm_min = 0;
        now_parts.tm_sec = 0;
        
        auto next = system_clock::from_time_t(mktime(&now_parts));
        auto till = next - now + milliseconds(100); // 100ms as a buffer
        
        {
            std::unique_lock orderLock { ordersMutex_ }; // prevent accessing of orders if its being used by main thread
            
            if (shutdown_.load(std::memory_order_acquire) || shutdownConditionVariable_.wait_for(orderLock, till) == std::cv_status::no_timeout)
                return;
        }
        
        OrderIds orderIds;
        
        {
            std::scoped_lock orderLock { ordersMutex_ };
            
            for (const auto& [_, entry] : orders_)
            {
                const auto& [order, _] = entry;
                
                if (order->GetOrderType() != OrderType::GoodForDay)
                    continue;
                
                orderIds.push_back(order->GetOrderId());
            }
        }
        
        CancelOrders(orderIds);
    }
}

void OrderBook::CancelOrders(OrderIds orderIds)
{
    std::scoped_lock orderLock { ordersMutex_ };
    
    for (const auto& orderId : orderIds)
        CancelOrderInternal(orderId);
}

void OrderBook::CancelOrderInternal(OrderId orderId)
{
    if (!orders_.contains(orderId))
        return;
    
    const auto [order, iter] = orders_.at(orderId);
    orders_.erase(orderId);
    
    if (order->GetSide() == Side::Sell)
    {
        auto price = order->GetPrice();
        auto& orders = asks_.at(price);
        orders.erase(iter);
        if (orders.empty())
            asks_.erase(price);
    }
    else
    {
        auto price = order->GetPrice();
        auto& orders = bids_.at(price);
        orders.erase(iter);
        if (orders.empty())
            bids_.erase(price);
    }
}

Trades OrderBook::AddOrder(OrderPointer order)
{
    // cannot add order if the orderId already exists
    if (orders_.contains(order->GetOrderId()))
        return {};
        
    if (order->GetOrderType() == OrderType::Market)
    {
        if (order->GetSide() == Side::Buy && !asks_.empty())
        {
            const auto& [worstAsk, _] = *asks_.end();
            order->ToGoodTillCancel(worstAsk);
        }
        else if (order->GetSide() == Side::Sell && !bids_.empty())
        {
            const auto& [worstBid, _] = *bids_.end();
            order->ToGoodTillCancel(worstBid);
        }
        else
            return {};
    }
    
    if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
        return {};
        
    OrderPointers::iterator iter;
    
    if (order->GetSide() == Side::Buy)
    {
        auto& orders = bids_[order->GetPrice()];
        orders.push_back(order);
        iter = std::next(orders.begin(), orders.size() - 1);
    }
    if (order->GetSide() == Side::Sell)
    {
        auto& orders = asks_[order->GetPrice()];
        orders.push_back(order);
        iter = std::next(orders.begin(), orders.size() - 1);
    }
    
    orders_.insert({order->GetOrderId(), OrderEntry{ order, iter }});
    
    return MatchOrders();
}

void OrderBook::CancelOrder(OrderId orderId)
{
    if (!orders_.contains(orderId))
        return;
    
    const auto& [order, orderIter] = orders_.at(orderId);
    orders_.erase(orderId);
    
    if (order->GetSide() == Side::Sell)
    {
        auto price = order->GetPrice();
        auto& orders = asks_.at(price); 
        orders.erase(orderIter);
        if (orders.empty())
            asks_.erase(price);
    }
    else
    {
        auto price = order->GetPrice();
        auto& orders = bids_.at(price);
        orders.erase(orderIter);
        if (orders.empty())
            bids_.erase(price);
    }
}

Trades OrderBook::MatchOrder(OrderModify order)
{
    if (!orders_.contains(order.GetOrderId()))
        return {};
    
    const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
    CancelOrder(order.GetOrderId());
    return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
}

std::size_t OrderBook::Size() const { return orders_.size(); }

OrderBookLevelInfos OrderBook::GetOrderInfos() const
{
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(orders_.size());
    askInfos.reserve(asks_.size());
    
    auto CreateLevelInfos = [](Price price, const OrderPointers& orders)
    {
        return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
            [](Quantity runningSum, const OrderPointer& order)
            { return runningSum + order->GetRemainingQuantity(); }) };
    };
    
    for (const auto& [price, orders] : bids_)
        bidInfos.push_back(CreateLevelInfos(price, orders));
    for (const auto& [price, orders] : asks_)
        askInfos.push_back(CreateLevelInfos(price, orders));
        
    return OrderBookLevelInfos{ bidInfos, askInfos };
}
    
bool OrderBook::CanMatch(Side side, Price price) const
{
    if (side == Side::Buy)
    {
        if (asks_.empty())
            return false;
        
        const auto& [bestAsk, _] = *asks_.begin(); // get first (best) ask
        return price >= bestAsk;
    }
    else
    {
        if (bids_.empty())
            return false;
        
        const auto& [bestBid, _] = *bids_.begin(); // get first (best) bid
        return price <= bestBid;
    }
}

Trades OrderBook::MatchOrders()
{
    Trades trades;
    trades.reserve(orders_.size());
    
    while (true)
    {
        if (bids_.empty() || asks_.empty())
            break;
            
        auto& [bidPrice, bids] = *bids_.begin();
        auto& [askPrice, asks] = *asks_.begin();
        
        if (bidPrice < askPrice)
            break;
            
        while (!bids_.empty() && !asks_.empty())
        {
            auto bid = bids.front();
            auto ask = asks.front();
            
            Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());
            
            bid->Fill(quantity);
            ask->Fill(quantity);
            
            if (bid->IsFilled())
            {
                bids.pop_front();
                orders_.erase(bid->GetOrderId());
            }
            
            if (ask->IsFilled())
            {
                asks.pop_front();
                orders_.erase(ask->GetOrderId());
            }
            
            if (bids_.empty())
            {
                bids_.erase(bidPrice);
            }
            if (asks_.empty())
            {
                asks_.erase(askPrice);
            }
            
            trades.push_back(Trade {
                TradeInfo { bid->GetOrderId(), bid->GetPrice(), quantity },
                TradeInfo { ask->GetOrderId(), ask->GetPrice(), quantity }
            });
        }
    }
    
    if (!bids_.empty())
    {
        auto& [_, bids] = *bids_.begin();
        auto& order = bids.front();
        if (order->GetOrderType()==OrderType::FillAndKill)
            CancelOrder(order->GetOrderId());
    }
    if (!asks_.empty())
    {
        auto& [_, asks] = *asks_.begin();
        auto& order = asks.front();
        if (order->GetOrderType()==OrderType::FillAndKill)
            CancelOrder(order->GetOrderId());
    }
    
    return trades;
}