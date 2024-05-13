#include <cstdint>
#include <vector>
#include <format>
#include <list>
#include <map>
#include <unordered_map>
#include <numeric>
#include <iostream>

enum class OrderType
{
    GoodTillCancel,
    FillAndKill,
};

enum class Side
{
    Buy,
    Sell,
};

using Price = std::int32_t;
using Quantity = std::int32_t;
using OrderId = std::int32_t;

struct LevelInfo
{
    Price price_;
    Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderBookLevelInfos
{
public:
    OrderBookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
        : bids_ { bids }
        , asks_{ asks }
        {}

    // data members returned by const ref because member functions always called on object
    const LevelInfos& GetBids() const { return bids_; }
    const LevelInfos& GetAsks() const { return asks_; }

private:
    LevelInfos bids_;
    LevelInfos asks_;
};

class Order
{
public:
    Order(const OrderType& orderType, OrderId orderId, Side side, Price price, Quantity quantity)
        : orderType_{ orderType }
        , orderId_{ orderId }
        , side_{ side }
        , price_{ price }
        , initialQuantity_{ quantity }
        , remainingQuantity_ { quantity }
        {}
    OrderType GetOrderType() const { return orderType_; }
    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
    bool IsFilled() const { return GetRemainingQuantity() == 0; }
    
    void Fill(Quantity quantity)
    {
        if (quantity > GetRemainingQuantity())
        {
            throw std::logic_error(std::format("Order ({}) cannot be filled for more than its remaning quantity.", GetOrderId()));
        }
        
        remainingQuantity_ -= quantity;
    }

private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

// reference semantics because the same order will be stored in different data structures
using OrderPointer = std::shared_ptr<Order>;
// because this is a list and we need to cancel orders later, we need some sort of iterator later on
using OrderPointers = std::list<OrderPointer>;

class OrderModify
{
public:
    OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
        : orderId_ { orderId }
        , side_ { side }
        , price_ { price }
        , quantity_ { quantity }
        {}
    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetQuantity() const { return quantity_; }
    
    OrderPointer ToOrderPointer(OrderType type) const
    {
        return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
    }

private:
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity quantity_;
};

struct TradeInfo
{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

class Trade
{
public:
    Trade (const TradeInfo& bidTrade, const TradeInfo& askTrade)
        : bidTrade_ { bidTrade }
        , askTrade_ { askTrade }
        {}
    const TradeInfo& GetBid() const { return bidTrade_; }
    const TradeInfo& GetAsk() const { return askTrade_; }
    
private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
};

using Trades = std::vector<Trade>;

class OrderBook
{
public:
    Trades AddOrder(OrderPointer order)
    {
        // cannot add order if the orderId already exists
        if (orders_.contains(order->GetOrderId()))
            return {};
        
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
    
    void CancelOrder(OrderId orderId)
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
    
    Trades MatchOrder(OrderModify order)
    {
        if (!orders_.contains(order.GetOrderId()))
            return {};
        
        const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
        CancelOrder(order.GetOrderId());
        return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
    }
    
    std::size_t Size() const { return orders_.size(); }
    
    OrderBookLevelInfos GetOrderInfos() const
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
    
    bool CanMatch(Side side, Price price) const
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
    
    Trades MatchOrders()
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
};

int main()
{
    OrderBook orderBook;
    OrderId orderId = 0;
    orderBook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId, Side::Buy, 100, 10));
    std::cout << orderBook.Size() << '\n';
    orderBook.CancelOrder(orderId);
    std::cout << orderBook.Size() << '\n';
    return 0;
}