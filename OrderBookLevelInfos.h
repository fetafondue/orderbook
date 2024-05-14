#ifndef ORDERBOOKLEVELINFOS_H
#define ORDERBOOKLEVELINFOS_H

#include "LevelInfo.h"

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

#endif