#include "../StoreFixtures.hpp"
#include "shop/common/CartRuleException.hpp"
#include "shop/common/DeliveryRuleException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/DuplicateOperationException.hpp"
#include "shop/common/FulfillmentStateException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/common/OrderStateException.hpp"
#include "shop/common/PaymentRuleException.hpp"
#include "shop/common/QuoteChangedException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

TEST_CASE(
    "Payment decline success and cancellation conserve reservations money and paid coupon quota") {
    auto state = test::store(true);
    const auto id = test::place(state);
    OrderProcessing process(state);
    const auto stockBefore = state.inventory();
    const auto couponsBefore = state.coupons();
    auto result = process.pay("alice", id, Money(110), PaymentMethod::SimulatedCard, false, "Отказ",
                              "decline");
    REQUIRE_FALSE(result.successful);
    REQUIRE(state.inventory() == stockBefore);
    REQUIRE(state.coupons() == couponsBefore);
    REQUIRE(state.orders().order(id)->status() == OrderStatus::AwaitingPayment);
    const auto declined = state;
    REQUIRE(process.pay("alice", id, Money(110), PaymentMethod::SimulatedCard, false, "Отказ",
                        "decline") == result);
    REQUIRE(state == declined);
    REQUIRE_THROWS_AS(
        process.pay("bob", id, Money(110), PaymentMethod::SimulatedCard, true, "", "foreign"),
        OrderStateException);
    REQUIRE_THROWS_AS(
        process.pay("alice", id, Money(111), PaymentMethod::SimulatedCard, true, "", "wrong"),
        PaymentRuleException);
    REQUIRE(state.payments() == declined.payments());
    REQUIRE(state.orders() == declined.orders());
    REQUIRE(state == declined);
    state.advanceClock(
        11); // Coupon and promotion expired after checkout; held purchase terms still apply.
    result =
        process.pay("alice", id, Money(110), PaymentMethod::SimulatedTransfer, true, "", "success");
    REQUIRE(result.successful);
    REQUIRE(state.coupons().confirmedUses().contains(id));
    REQUIRE(state.coupons().heldUses().empty());
    REQUIRE(state.inventory().reservation(state.orders().order(id)->reservationId()).status() ==
            ReservationStatus::Confirmed);
    const auto paid = state;
    REQUIRE(process.pay("alice", id, Money(110), PaymentMethod::SimulatedTransfer, true, "",
                        "success") == result);
    REQUIRE(state == paid);
    process.startPicking(id, "start");
    process.pick(id, "shirt-blue", 1, "pick");
    process.cancel("alice", id, "cancel");
    REQUIRE(state.orders().order(id)->status() == OrderStatus::Cancelled);
    REQUIRE(state.payments().refundedForOrder(id) == Money(110));
    REQUIRE(state.inventory().item("shirt-blue").reserved() == 0);
    REQUIRE(state.inventory().item("shirt-blue").onHand() == 20);
    REQUIRE(state.coupons().confirmedUses().contains(id));
    REQUIRE(state.fulfillments().at(id).status() == FulfillmentStatus::Cancelled);
    REQUIRE(state.payments().reconcileOrderBalance(*state.orders().order(id)).empty());
    REQUIRE(state.inventory().auditReservations(state.orders()).empty());
    const auto cancelled = state;
    process.cancel("alice", id, "cancel");
    REQUIRE(state == cancelled);
}

TEST_CASE(
    "Rejected payment parameters preserve the counter and allow retry with the same operation ID") {
    auto state = test::store();
    const auto id = test::place(state);
    OrderProcessing process(state);
    const auto before = state;
    REQUIRE_THROWS_AS(
        process.pay("alice", id, Money(1), PaymentMethod::SimulatedCard, true, "", "retry"),
        PaymentRuleException);
    REQUIRE(state == before);
    REQUIRE_THROWS_AS(
        process.pay("alice", id, Money(1), PaymentMethod::SimulatedCard, false, "Отказ", "retry"),
        PaymentRuleException);
    REQUIRE(state == before);
    REQUIRE_THROWS_AS(
        process.pay("alice", id, Money(110), static_cast<PaymentMethod>(99), true, "", "retry"),
        InvalidDomainDataException);
    REQUIRE(state == before);
    const auto result =
        process.pay("alice", id, Money(110), PaymentMethod::SimulatedCard, true, "", "retry");
    REQUIRE(result.attemptId == "attempt-3");
    REQUIRE(result.paymentId == "payment-4");
    const auto paid = state;
    REQUIRE(process.pay("alice", id, Money(110), PaymentMethod::SimulatedCard, true, "", "retry") ==
            result);
    REQUIRE(state == paid);
}

TEST_CASE("Unpaid cancellation releases coupon and payment deadline extension stays synchronized") {
    auto state = test::store(true);
    const auto id = test::place(state);
    OrderProcessing process(state);
    const auto originalDeadline = state.orders().order(id)->paymentDeadline();
    process.extendPaymentDeadline("alice", id, "extend");
    REQUIRE(state.orders().order(id)->paymentDeadline() == originalDeadline + 20);
    REQUIRE(state.inventory().reservation(state.orders().order(id)->reservationId()).expiresAt() ==
            originalDeadline + 20);
    const auto before = state;
    process.extendPaymentDeadline("alice", id, "extend");
    REQUIRE(state == before);
    REQUIRE_THROWS_AS(process.extendPaymentDeadline("alice", id, "again"), OrderStateException);
    REQUIRE(state == before);
    process.cancel("alice", id, "cancel");
    REQUIRE(state.coupons().heldUses().empty());
    REQUIRE(state.payments().refunds().empty());
    REQUIRE(state.inventory().auditReservations(state.orders()).empty());
}

TEST_CASE("Shipping consumes stock exactly once and receipt credits only delivered goods") {
    auto state = test::store();
    const auto id = test::place(state);
    test::pay(state, id);
    OrderProcessing process(state);
    process.startPicking(id, "start");
    const auto started = state;
    REQUIRE_THROWS_AS(process.startPicking(id, "another-start"), FulfillmentStateException);
    REQUIRE(state == started);
    process.pick(id, "shirt-blue", 2, "pick1");
    const auto partial = state;
    REQUIRE_THROWS_AS(process.pack(id, "pack"), FulfillmentStateException);
    REQUIRE(state == partial);
    process.pick(id, "shirt-blue", 1, "pick2");
    process.pack(id, "pack");
    process.reopenPicking(id, "reopen");
    process.undoPick(id, "shirt-blue", 1, "undo");
    process.pick(id, "shirt-blue", 1, "fix");
    process.pack(id, "pack2");
    REQUIRE(state.inventory().item("shirt-blue").onHand() == 20);
    REQUIRE(state.loyaltyAccount("alice").qualifyingSpend() == Money{});
    process.dispatch(id, "TRACK", "dispatch");
    REQUIRE(state.inventory().item("shirt-blue").onHand() == 17);
    REQUIRE(state.inventory().item("shirt-blue").reserved() == 0);
    const auto shipped = state;
    process.dispatch(id, "TRACK", "dispatch");
    REQUIRE(state == shipped);
    REQUIRE_THROWS_AS(process.cancel("alice", id, "late-cancel"), OrderStateException);
    REQUIRE(state == shipped);
    REQUIRE_THROWS_AS(process.confirmDelivery("bob", id, "foreign"), OrderStateException);
    REQUIRE(state == shipped);
    process.confirmDelivery("alice", id, "deliver");
    REQUIRE(state.loyaltyAccount("alice").qualifyingSpend() == Money(100));
    REQUIRE(state.orders().order(id)->status() == OrderStatus::Delivered);
    REQUIRE(state.inventory().auditReservations(state.orders()).empty());
    const auto delivered = state;
    process.confirmDelivery("alice", id, "deliver");
    REQUIRE(state == delivered);
    REQUIRE_THROWS_AS(process.confirmDelivery("alice", id, "again"), OrderStateException);
}

TEST_CASE("Unpaid delivery changes use frozen goods and require exact confirmation") {
    auto state = test::store();
    const auto id = test::place(state);
    OrderProcessing process(state);
    const auto original = state.orders().order(id);
    state.changePrice("shirt-blue", Money(999));
    const auto terms = process.previewUnpaidDelivery("alice", id, test::address(), "express");
    REQUIRE(terms.fee == Money(20));
    auto wrong = terms;
    wrong.fee = Money(1);
    const auto before = state;
    REQUIRE_THROWS_AS(process.changeUnpaidDelivery("alice", id, wrong, "change"),
                      QuoteChangedException);
    REQUIRE(state == before);
    process.changeUnpaidDelivery("alice", id, terms, "change");
    REQUIRE(state.orders().order(id)->total() == Money(120));
    REQUIRE(state.orders().order(id)->goodsNet() == Money(100));
    REQUIRE(state.orders().order(id)->paymentDeadline() == original->paymentDeadline());
    REQUIRE(state.orders().order(id)->lines() == original->lines());
    REQUIRE_THROWS_AS(
        process.pay("alice", id, Money(110), PaymentMethod::SimulatedCard, true, "", "old-amount"),
        PaymentRuleException);
    test::pay(state, id);
    REQUIRE(state.payments().reconcileOrderBalance(*state.orders().order(id)).empty());
}

TEST_CASE("Repeat-order selection uses current availability without copying old promotions") {
    auto state = test::store();
    const auto id = test::place(state);
    OrderProcessing process(state);
    REQUIRE(process.repeatOrderPlan("alice", id).front().available);
    process.repeatOrder("alice", id, {"shirt-blue"}, "repeat");
    REQUIRE(state.cart("alice").totalUnits() == 3);
    REQUIRE_FALSE(state.cart("alice").couponCode());
    const auto repeated = state;
    process.repeatOrder("alice", id, {"shirt-blue"}, "repeat");
    REQUIRE(state == repeated);
    REQUIRE_THROWS_AS(process.repeatOrder("alice", id, {"shirt-blue", "shirt-blue"}, "duplicates"),
                      DuplicateEntityException);
    auto catalog = state.catalog();
    catalog.archiveProduct("shirt");
    state.replaceCatalog(catalog);
    REQUIRE_FALSE(process.repeatOrderPlan("alice", id).front().available);
    REQUIRE_THROWS_AS(process.repeatOrder("alice", id, {"shirt-blue"}, "unavailable"),
                      CartRuleException);
}

TEST_CASE("Free-delivery edit cannot turn a fully discounted goods purchase into a zero order") {
    auto state = test::store();
    const Promotion full("full", "Полная скидка", PromotionKind::Percentage, 10000, Money{},
                         TimeWindow(0, 100), {}, false);
    state.addPromotion(full);
    state.registerCoupon(Coupon("FULL", "full", 1, 1, TimeWindow(0, 100)));
    auto cart = state.cart("alice");
    cart.setCoupon("FULL");
    state.replaceCart(cart);
    const auto id = test::place(state);
    REQUIRE(state.orders().order(id)->total() == Money(10));
    OrderProcessing process(state);
    const auto before = state;
    REQUIRE_THROWS_AS(process.previewUnpaidDelivery("alice", id, test::address(), "free"),
                      DeliveryRuleException);
    REQUIRE_THROWS_AS(
        process.changeUnpaidDelivery("alice", id, {test::address(), "free", Money{}}, "zero"),
        DeliveryRuleException);
    REQUIRE(state == before);
}

TEST_CASE(
    "Tracking numbers are unique across shipments and restrictions preserve earlier obligations") {
    auto state = test::store();
    const auto first = test::place(state);
    test::pay(state, first);
    test::ship(state, first);
    OrderProcessing process(state);
    process.repeatOrder("alice", first, {"shirt-blue"}, "repeat");
    const auto second = test::place(state, "place2");
    test::pay(state, second, "pay2");
    process.startPicking(second, "start2");
    process.pick(second, "shirt-blue", 3, "pick2");
    process.pack(second, "pack2");
    const auto before = state;
    REQUIRE_THROWS_AS(process.dispatch(second, "TRACK-1", "same-track"), DuplicateEntityException);
    REQUIRE(state == before);
    auto customer = state.customer("alice");
    customer.restrictPurchases("Проверка");
    state.replaceCustomer(customer);
    process.dispatch(second, "TRACK-2", "dispatch2");
    process.confirmDelivery("alice", first, "deliver1");
    REQUIRE(state.loyaltyAccount("alice").qualifyingSpend() == Money(100));
    process.confirmDelivery("alice", second, "deliver2");
    REQUIRE(state.loyaltyAccount("alice").qualifyingSpend() == Money(200));
    REQUIRE(state.inventory().auditReservations(state.orders()).empty());
}
