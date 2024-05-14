#ifndef ORDERTYPE_H
#define ORDERTYPE_H

enum class OrderType
{
    GoodTillCancel,
    FillAndKill,
    FillOrKill,
    GoodForDay,
    Market,
};

#endif