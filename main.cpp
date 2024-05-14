#include <iostream>

#include "OrderBook.h"

int main()
{
    OrderBook orderBook;
    const OrderId orderId = 1;
    orderBook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, orderId, Side::Buy, 100, 10));
    std::cout << orderBook.Size() << '\n'; // 1
    orderBook.CancelOrder(orderId);
    std::cout << orderBook.Size() << '\n'; // 0
    return 0;
}