#include "../PurchaseFixtures.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/EntityNotFoundException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/common/OrderStateException.hpp"
#include "shop/orders/OrderBook.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

TEST_CASE("Order lines preserve all purchased facts and reconstruct delivery input") {
    const auto order = test::order();
    const auto line = order.line("shirt-blue");
    REQUIRE(line.sku() == "shirt-blue");
    REQUIRE(line.title() == "Футболка");
    REQUIRE(line.quantity() == 3);
    REQUIRE(line.unitPrice() == Money(40));
    REQUIRE(line.allocation().discountTotal() == Money(20));
    REQUIRE(line.netTotal() == Money(100));
    REQUIRE(line.weightGrams() == 200);
    REQUIRE(line.attributesSnapshot().at("color") == "blue");
    REQUIRE(line.dimensionsMm()[0] == 200);
    REQUIRE(line.returnEligible());
    REQUIRE(line.asQuoteLine() == test::quote().lines().front());
    REQUIRE(order.lines().size() == 1);
    REQUIRE(order.quantities().at("shirt-blue") == 3);
    REQUIRE(order.quoteLines() == test::quote().lines());
    REQUIRE(order.id() == "order");
    REQUIRE(order.customerId() == "alice");
    REQUIRE(order.goodsNet() == Money(100));
    REQUIRE(order.total() == Money(110));
    REQUIRE(order.deliveryAddress() == test::address());
    REQUIRE(order.deliveryOptionId() == "standard");
    REQUIRE(order.deliveryFee() == Money(10));
    REQUIRE(order.discountSource() == test::quote().discountSource());
    REQUIRE(order.createdAt() == 10);
    REQUIRE(order.paymentDeadline() == 100);
    REQUIRE(order.reservationId() == "reservation-order");
    REQUIRE(order.returnPolicy() == ReturnPolicy(100));
    REQUIRE_FALSE(order.deadlineExtended());
    REQUIRE_FALSE(order.deliveredAt());
    REQUIRE_THROWS_AS(order.line("missing"), EntityNotFoundException);
    REQUIRE_THROWS_AS(order.assertOwner("bob"), OrderStateException);
    REQUIRE_NOTHROW(order.assertOwner("alice"));
}

TEST_CASE("Order transitions preserve history and reject time travel and illegal phases") {
    auto order = test::order();
    const auto before = order;
    REQUIRE_THROWS_AS(order.confirmPayment(9, "pay", "p"), OrderStateException);
    REQUIRE_THROWS_AS(order.confirmPayment(100, "pay", "p"), OrderStateException);
    REQUIRE_THROWS_AS(order.expire(99, "expire"), OrderStateException);
    REQUIRE_THROWS_AS(order.beginFulfillment(12, "pick", "f"), OrderStateException);
    REQUIRE_THROWS_AS(order.recordReturnCompleted(12, "return", "r"), OrderStateException);
    REQUIRE(order == before);
    order.confirmPayment(11, "pay", "p");
    REQUIRE(order.status() == OrderStatus::Confirmed);
    order.beginFulfillment(12, "pick", "f");
    REQUIRE(order.status() == OrderStatus::InFulfillment);
    order.markShipped(13, "ship", "f");
    REQUIRE(order.status() == OrderStatus::Shipped);
    REQUIRE_THROWS_AS(order.cancel(14, "cancel"), OrderStateException);
    order.markDelivered(14, "deliver", "f");
    REQUIRE(order.status() == OrderStatus::Delivered);
    REQUIRE(order.deliveredAt() == 14);
    order.recordReturnCompleted(15, "return", "r");
    REQUIRE_THROWS_AS(order.recordReturnCompleted(16, "again", "r"), OrderStateException);
    REQUIRE(order.history().back().relatedEntityId() == "r");
    REQUIRE(order.history().back().operationId() == "return");
    REQUIRE(order.stateBefore(11).status == OrderStatus::AwaitingPayment);
    REQUIRE(order.stateBefore(12).status == OrderStatus::Confirmed);
    REQUIRE(order.stateBefore(13).status == OrderStatus::InFulfillment);
    REQUIRE(order.stateBefore(14).status == OrderStatus::Shipped);
    REQUIRE(order.stateBefore(16).status == OrderStatus::Delivered);
    REQUIRE_THROWS_AS(order.stateBefore(10), InvalidDomainDataException);
    auto expired = before;
    expired.expire(100, "expiry");
    REQUIRE(expired.status() == OrderStatus::Expired);
    REQUIRE(expired.stateBefore(101).status == OrderStatus::Expired);
    auto cancelled = before;
    cancelled.cancel(11, "cancel");
    REQUIRE(cancelled.status() == OrderStatus::Cancelled);
    REQUIRE(cancelled.stateBefore(12).status == OrderStatus::Cancelled);
    REQUIRE_THROWS_AS(cancelled.cancel(12, "again"), OrderStateException);
}

TEST_CASE("Unpaid delivery edits and one-time extension retain historical commercial terms") {
    auto order = test::order();
    const auto original = order;
    const Address changed("BY", "220002", "Минск", "Новая", "2");
    order.changeUnpaidDelivery(11, {changed, "express", Money(20)}, "change");
    REQUIRE(order.total() == Money(120));
    REQUIRE(order.lines() == original.lines());
    REQUIRE(order.paymentDeadline() == 100);
    REQUIRE(order.stateBefore(11).delivery.address == test::address());
    REQUIRE(order.stateBefore(12).delivery.address == changed);
    REQUIRE(order.stateBefore(12).delivery.fee == Money(20));
    order.extendPaymentDeadline(12, 50, "extend");
    REQUIRE(order.paymentDeadline() == 150);
    REQUIRE(order.deadlineExtended());
    REQUIRE(order.stateBefore(12).paymentDeadline == 100);
    REQUIRE(order.stateBefore(13).paymentDeadline == 150);
    const auto before = order;
    REQUIRE_THROWS_AS(order.extendPaymentDeadline(13, 1, "twice"), OrderStateException);
    REQUIRE_THROWS_AS(order.changeUnpaidDelivery(13, {changed, "x", Money(-1)}, "bad"),
                      InvalidDomainDataException);
    REQUIRE(order == before);
    const auto& event = order.history().front();
    REQUIRE(event.kind() == OrderEventKind::Created);
    REQUIRE(event.timestamp() == 10);
    REQUIRE(event.description() == "Заказ оформлен");
    REQUIRE_THROWS_AS(OrderEvent(0, OrderEventKind::Created, "bad"), InvalidDomainDataException);
    REQUIRE_THROWS_AS(OrderEvent(-1, OrderEventKind::Cancelled, "bad"), InvalidDomainDataException);
    REQUIRE_THROWS_AS(OrderEvent(0, static_cast<OrderEventKind>(99), "bad"),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(
        OrderEvent(1, OrderEventKind::DeadlineExtended, "bad", "", "", DeadlineChange{2, 1}),
        InvalidDomainDataException);
}

TEST_CASE(
    "OrderBook immutable snapshots survive replacements and copies have independent indices") {
    OrderBook book;
    auto entry = test::order();
    book.add(entry);
    const auto snapshot = book.order("order");
    const auto copy = book;
    entry.confirmPayment(11, "pay", "p");
    book.replace(entry);
    REQUIRE(snapshot->status() == OrderStatus::AwaitingPayment);
    REQUIRE(copy.order("order")->status() == OrderStatus::AwaitingPayment);
    REQUIRE(book.order("order")->status() == OrderStatus::Confirmed);
    REQUIRE(book.revision() == 2);
    REQUIRE(book.orders().size() == 1);
    REQUIRE_THROWS_AS(book.add(entry), DuplicateEntityException);
    REQUIRE_THROWS_AS(book.replace(test::order()), OrderStateException);
    REQUIRE_THROWS_AS(book.order("missing"), EntityNotFoundException);
    book.add(test::order("a", 20));
    book.add(test::order("b", 20));
    const auto history = book.customerHistory("alice");
    REQUIRE(history.size() == 3);
    REQUIRE(history[0]->id() == "a");
    REQUIRE(history[1]->id() == "b");
    REQUIRE(history[2]->id() == "order");
    REQUIRE(book.customerHistory("missing").empty());
    REQUIRE(book.customerHistory("alice", OrderStatus::Confirmed).size() == 1);
    REQUIRE(book.customerHistory("alice", std::nullopt, TimeWindow(10, 20)).size() == 1);
    REQUIRE(book.selectForPeriod(TimeWindow(20, 21)).size() == 2);
    REQUIRE(book.findOverdueUnpaid(99).empty());
    REQUIRE(book.findOverdueUnpaid(100) == std::vector<std::string>{"a", "b"});
    REQUIRE(copy == OrderBook(copy));
    REQUIRE_FALSE(copy == book);
}
