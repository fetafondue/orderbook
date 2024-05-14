#ifndef TRADE_H
#define TRADE_H

#include "TradeInfo.h"

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

#endif