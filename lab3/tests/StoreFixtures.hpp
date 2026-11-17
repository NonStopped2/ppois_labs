#pragma once

#include "Fixtures.hpp"
#include "shop/application/Checkout.hpp"
#include "shop/application/OrderProcessing.hpp"

namespace shop::test {
inline StoreState store(bool withCoupon = false) {
    const DeliveryOption standard("standard", "Стандарт", {"22"}, 10000, 1000, Money(10),
                                  Money(1000));
    const DeliveryOption express("express", "Экспресс", {"22"}, 10000, 1000, Money(20),
                                 Money(1000));
    const DeliveryOption free("free", "Самовывоз", {}, 10000, 1000, Money{}, Money{});
    StoreState result(
        Inventory(Warehouse("warehouse", "Склад", address())),
        DeliveryPolicy({standard, express, free}, 10,
                       {{"standard", {100, 200}}, {"express", {10, 20}}, {"free", {0, 1}}}),
        10, PricingPolicy{}, LoyaltyPolicy{}, ReturnPolicy(100), 30, 20);
    result.replaceCatalog(catalog());
    result.addCustomer(customer());
    result.addCustomer(customer("bob"));
    result.changePrice("shirt-blue", Money(40));
    result.addPromotion(Promotion("sale", "Акция", PromotionKind::FixedAmount, 20, Money{},
                                  TimeWindow(0, 20), {}, !withCoupon));
    if (withCoupon) {
        result.registerCoupon(Coupon("SAVE", "sale", 1, 1, TimeWindow(0, 20)));
    }
    StockReceipt receipt("receipt", "supplier", "warehouse");
    receipt.addLine(ReceiptLine("shirt-blue", 20, Money(10), "batch"));
    result.postReceipt(receipt);
    auto cart = result.cart("alice");
    cart.addItem(result.catalog(), "shirt-blue", 3, result.now());
    if (withCoupon) {
        cart.setCoupon("SAVE");
    }
    result.replaceCart(cart);
    return result;
}
inline std::string place(StoreState& state, std::string_view operation = "place") {
    Checkout checkout(state);
    return checkout.placeOrder("alice", checkout.preview("alice", address(), "standard"),
                               operation);
}
inline void pay(StoreState& state, std::string_view id, std::string_view operation = "pay") {
    OrderProcessing(state).pay("alice", id, state.orders().order(id)->total(),
                               PaymentMethod::SimulatedCard, true, "", operation);
}
inline void ship(StoreState& state, std::string_view id) {
    OrderProcessing process(state);
    process.startPicking(id, "start");
    for (const auto& [sku, quantity] : state.orders().order(id)->quantities()) {
        process.pick(id, sku, quantity, "pick-" + sku);
    }
    process.pack(id, "pack");
    process.dispatch(id, "TRACK-1", "dispatch");
}
} // namespace shop::test
