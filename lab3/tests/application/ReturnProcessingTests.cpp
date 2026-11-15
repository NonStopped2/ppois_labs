#include "../StoreFixtures.hpp"
#include "shop/application/ReturnProcessing.hpp"
#include "shop/common/DuplicateOperationException.hpp"
#include "shop/common/OrderStateException.hpp"
#include "shop/common/ReturnRuleException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;
namespace {
std::string deliver(StoreState& state) {
    auto id = test::place(state);
    test::pay(state, id);
    test::ship(state, id);
    OrderProcessing(state).confirmDelivery("alice", id, "deliver");
    return id;
}
} // namespace

TEST_CASE("Consecutive partial returns conserve every kopeck but restock only eligible goods") {
    auto state = test::store(true);
    const auto orderId = deliver(state);
    ReturnProcessing returns(state);
    REQUIRE(state.loyaltyAccount("alice").qualifyingSpend() == Money(100));
    REQUIRE(state.inventory().item("shirt-blue").onHand() == 17);
    const auto frozen = state.orders().order(orderId);
    for (int part = 0; part < 3; ++part) {
        const auto suffix = std::to_string(part);
        const auto id = returns.open("alice", orderId, {{"shirt-blue", 1, ReturnReason::Defective}},
                                     "open" + suffix);
        REQUIRE(state.returnRequests().at(id).lines().front().alreadyReturnedOffset() == part);
        const bool damaged = part == 1;
        returns.inspect(
            id,
            {InspectionResult("shirt-blue", 1, 1, damaged ? 0 : 1,
                              damaged ? ItemCondition::Defective : ItemCondition::Opened)},
            "inspect" + suffix);
        const auto expected = part == 0 ? Money(34) : Money(33);
        REQUIRE(returns.complete(id, "complete" + suffix) == expected);
        const auto committed = state;
        REQUIRE(returns.complete(id, "complete" + suffix) == expected);
        REQUIRE(state == committed);
        REQUIRE(state.returnRequests().at(id).status() == ReturnStatus::Completed);
        REQUIRE(state.returnRequests().at(id).refundId().has_value());
    }
    REQUIRE(returns.completedQuantities(orderId).at("shirt-blue") == 3);
    REQUIRE(state.payments().refundedForOrder(orderId) == Money(100));
    REQUIRE(state.inventory().item("shirt-blue").onHand() == 19);
    REQUIRE(state.loyaltyAccount("alice").qualifyingSpend() == Money{});
    REQUIRE(state.coupons().confirmedUses().contains(orderId));
    REQUIRE(state.payments().reconcileOrderBalance(*state.orders().order(orderId)).empty());
    REQUIRE(state.orders().order(orderId)->status() == OrderStatus::Delivered);
    REQUIRE(frozen->history().size() + 3 == state.orders().order(orderId)->history().size());
    REQUIRE_THROWS_AS(
        returns.open("alice", orderId, {{"shirt-blue", 1, ReturnReason::ChangedMind}}, "too-many"),
        ReturnRuleException);
}

TEST_CASE(
    "Submitted returns can be withdrawn amended or rejected without money and stock effects") {
    auto state = test::store();
    const auto orderId = deliver(state);
    ReturnProcessing returns(state);
    const auto id =
        returns.open("alice", orderId, {{"shirt-blue", 1, ReturnReason::ChangedMind}}, "open");
    const auto originalDeadline = state.returnRequests().at(id).deadline();
    const auto before = state;
    REQUIRE_THROWS_AS(
        returns.open("alice", orderId, {{"shirt-blue", 1, ReturnReason::ChangedMind}}, "second"),
        ReturnRuleException);
    REQUIRE_THROWS_AS(returns.withdraw("bob", id, "foreign"), ReturnRuleException);
    REQUIRE(state == before);
    returns.amendSubmitted("alice", id, {{"shirt-blue", 2, ReturnReason::ChangedMind}}, "amend");
    REQUIRE(state.returnRequests().at(id).maximumRefund() == Money(67));
    REQUIRE(state.returnRequests().at(id).deadline() == originalDeadline);
    returns.withdraw("alice", id, "withdraw");
    REQUIRE(state.returnRequests().at(id).status() == ReturnStatus::Withdrawn);
    const auto second =
        returns.open("alice", orderId, {{"shirt-blue", 1, ReturnReason::ChangedMind}}, "open2");
    returns.reject(second, "Не подтверждена причина", "reject");
    REQUIRE(state.returnRequests().at(second).status() == ReturnStatus::Rejected);
    REQUIRE(state.inventory() == before.inventory());
    REQUIRE(state.payments() == before.payments());
    REQUIRE(state.loyaltyAccount("alice") == before.loyaltyAccount("alice"));
    const auto third =
        returns.open("alice", orderId, {{"shirt-blue", 1, ReturnReason::ChangedMind}}, "open3");
    returns.inspect(third, {InspectionResult("shirt-blue", 1, 0, 0, ItemCondition::Incomplete)},
                    "inspect3");
    REQUIRE(state.returnRequests().at(third).status() == ReturnStatus::Rejected);
    REQUIRE_THROWS_AS(returns.complete(third, "complete3"), ReturnRuleException);
}

TEST_CASE(
    "Inspection failure leaves no document and deadline cleanup is an explicit separate action") {
    auto state = test::store();
    const auto orderId = deliver(state);
    ReturnProcessing returns(state);
    const auto id =
        returns.open("alice", orderId, {{"shirt-blue", 2, ReturnReason::Defective}}, "open");
    const auto before = state;
    REQUIRE_THROWS_AS(
        returns.inspect(id, {InspectionResult("shirt-blue", 1, 1, 0, ItemCondition::Defective)},
                        "partial"),
        ReturnRuleException);
    REQUIRE(state.returnRequests() == before.returnRequests());
    REQUIRE(state.inspections() == before.inspections());
    REQUIRE(state.payments() == before.payments());
    REQUIRE(state.inventory() == before.inventory());
    state.advanceClock(100);
    REQUIRE_THROWS_AS(
        returns.inspect(id, {InspectionResult("shirt-blue", 2, 2, 0, ItemCondition::Defective)},
                        "late"),
        ReturnRuleException);
    REQUIRE(returns.closeExpiredUnreceived() == std::vector<std::string>{id});
    REQUIRE(state.returnRequests().at(id).decisionReason() == "NotReceivedInTime");
    REQUIRE(returns.closeExpiredUnreceived().empty());
    REQUIRE_THROWS_AS(
        returns.open("alice", orderId, {{"shirt-blue", 1, ReturnReason::Defective}}, "expired"),
        ReturnRuleException);
    REQUIRE(state.payments().refunds().empty());
    REQUIRE(state.inventory() == before.inventory());
}

TEST_CASE("Zero-money accepted returns still record completed quantity and a historical event") {
    auto state = test::store();
    const Promotion full("full", "Полная скидка", PromotionKind::Percentage, 10000, Money{},
                         TimeWindow(0, 100), {}, false);
    state.addPromotion(full);
    state.registerCoupon(Coupon("FULL", "full", 1, 1, TimeWindow(0, 100)));
    auto cart = state.cart("alice");
    cart.setCoupon("FULL");
    state.replaceCart(cart);
    const auto orderId = deliver(state);
    ReturnProcessing returns(state);
    const auto id =
        returns.open("alice", orderId, {{"shirt-blue", 1, ReturnReason::ChangedMind}}, "open");
    state.advanceClock(1);
    returns.inspect(id, {InspectionResult("shirt-blue", 1, 1, 1, ItemCondition::Opened)},
                    "inspect");
    state.advanceClock(
        200); // Timely received inspection can be completed after the request deadline.
    REQUIRE(returns.closeExpiredUnreceived().empty());
    REQUIRE(returns.complete(id, "complete") == Money{});
    REQUIRE_FALSE(state.returnRequests().at(id).refundId());
    REQUIRE(state.payments().refunds().empty());
    REQUIRE(returns.completedQuantities(orderId).at("shirt-blue") == 1);
    REQUIRE(state.orders().order(orderId)->history().back().kind() ==
            OrderEventKind::ReturnCompleted);
    REQUIRE(state.orders().order(orderId)->history().back().timestamp() == 211);
    REQUIRE(state.inventory().item("shirt-blue").onHand() == 18);
}

TEST_CASE(
    "Return command replay verifies exact actor and input even after final state transitions") {
    auto state = test::store();
    const auto orderId = deliver(state);
    ReturnProcessing returns(state);
    const std::vector<ReturnSelection> selection{{"shirt-blue", 1, ReturnReason::ChangedMind}};
    const auto id = returns.open("alice", orderId, selection, "open");
    returns.withdraw("alice", id, "withdraw");
    const auto before = state;
    REQUIRE(returns.open("alice", orderId, selection, "open") == id);
    REQUIRE(state == before);
    returns.withdraw("alice", id, "withdraw");
    REQUIRE(state == before);
    REQUIRE_THROWS_AS(returns.open("bob", orderId, selection, "open"), DuplicateOperationException);
    REQUIRE_THROWS_AS(
        returns.open("alice", orderId, {{"shirt-blue", 2, ReturnReason::ChangedMind}}, "open"),
        DuplicateOperationException);
    REQUIRE_THROWS_AS(returns.open("bob", orderId, selection, "foreign"), OrderStateException);
}
