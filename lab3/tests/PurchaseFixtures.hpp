#pragma once

#include "Fixtures.hpp"
#include "shop/orders/Order.hpp"

namespace shop::test {
inline PriceQuote quote() {
    const auto values = catalog();
    return PriceQuote(
        "alice",
        {QuoteLine(values.variant("shirt-blue"), "Футболка", 3, Money(40), Money(20), true)},
        address(), "standard", Money(10), {DiscountKind::Promotion, "sale", std::nullopt}, 10, 100,
        ReturnPolicy(100));
}
inline Order order(std::string_view id = "order", Timestamp createdAt = 10) {
    return Order(id, quote(), "reservation-" + std::string(id), createdAt, 100,
                 "place-" + std::string(id));
}
inline Order paidOrder() {
    auto result = order();
    result.confirmPayment(11, "pay", "payment");
    return result;
}
inline Order deliveredOrder() {
    auto result = paidOrder();
    result.beginFulfillment(12, "pick", "fulfillment");
    result.markShipped(13, "ship", "fulfillment");
    result.markDelivered(14, "deliver", "fulfillment");
    return result;
}
} // namespace shop::test
