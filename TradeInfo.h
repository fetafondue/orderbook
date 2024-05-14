#ifndef TRADEINFO_H
#define TRADEINFO_H

#include "Alias.h"

struct TradeInfo
{
    OrderId orderId_;
    Price price_;
    Quantity quantity_;
};

#endif