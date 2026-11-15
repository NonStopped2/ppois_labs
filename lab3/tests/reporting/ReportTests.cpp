#include "../PurchaseFixtures.hpp"
#include "../StoreFixtures.hpp"
#include "shop/application/ReturnProcessing.hpp"
#include "shop/common/ReportRuleException.hpp"
#include "shop/reporting/SalesReport.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;
namespace {
std::string deliveredPurchase(StoreState& state, std::string_view actor, const std::string& tag) {
    auto cart = state.cart(actor);
    cart.clear();
    cart.addItem(state.catalog(), "shirt-blue", 1, state.now());
    state.replaceCart(cart);
    Checkout checkout(state);
    OrderProcessing process(state);
    auto id = checkout.placeOrder(actor, checkout.preview(actor, test::address(), "standard"),
                                  "place-" + tag);
    process.pay(actor, id, state.orders().order(id)->total(), PaymentMethod::SimulatedCard, true,
                "", "pay-" + tag);
    process.startPicking(id, "start-" + tag);
    process.pick(id, "shirt-blue", 1, "pick-" + tag);
    process.pack(id, "pack-" + tag);
    process.dispatch(id, "TRACK-" + tag, "ship-" + tag);
    process.confirmDelivery(actor, id, "deliver-" + tag);
    return id;
}
std::string returnUnits(StoreState& state, const std::string& orderId, std::int64_t quantity,
                        const std::string& tag) {
    ReturnProcessing process(state);
    auto id = process.open("alice", orderId, {{"shirt-blue", quantity, ReturnReason::Defective}},
                           "open-" + tag);
    process.inspect(
        id, {InspectionResult("shirt-blue", quantity, quantity, 0, ItemCondition::Defective)},
        "inspect-" + tag);
    process.complete(id, "complete-" + tag);
    return id;
}
} // namespace

TEST_CASE(
    "Reports separate cash operations from delivery cohorts and completion from inspection time") {
    auto state = test::store();
    const auto id = test::place(state);
    test::pay(state, id);
    state.advanceClock(10);
    test::ship(state, id);
    OrderProcessing(state).confirmDelivery("alice", id, "deliver");
    ReturnProcessing process(state);
    const auto request =
        process.open("alice", id, {{"shirt-blue", 1, ReturnReason::Defective}}, "open");
    state.advanceClock(1);
    process.inspect(request, {InspectionResult("shirt-blue", 1, 1, 0, ItemCondition::Defective)},
                    "inspect");
    const auto beforeComplete =
        SalesReport::buildFinancialSummary(TimeWindow(0, 21), state.reportContext());
    state.advanceClock(4);
    REQUIRE(process.complete(request, "complete") == Money(34));
    state.advanceClock(5);
    const auto early = SalesReport::buildFinancialSummary(TimeWindow(0, 15), state.reportContext());
    REQUIRE(early.period() == TimeWindow(0, 15));
    REQUIRE(early.captured() == Money(110));
    REQUIRE(early.refunded() == Money{});
    REQUIRE(early.deliveryRevenue() == Money(10));
    REQUIRE(early.deliveredNetGoods() == Money{});
    REQUIRE(early.orderStatusCounts().at(OrderStatus::Confirmed) == 1);
    REQUIRE(early.skuStatistics().empty());
    REQUIRE(early.reconcile(state.reportContext()).empty());
    const auto historical =
        SalesReport::buildFinancialSummary(TimeWindow(0, 21), state.reportContext());
    REQUIRE(historical.skuStatistics() == beforeComplete.skuStatistics());
    const auto inspected =
        SalesReport::buildFinancialSummary(TimeWindow(20, 24), state.reportContext());
    REQUIRE(inspected.deliveredNetGoods() == Money(100));
    REQUIRE(inspected.returnRate() == ExactRatio{0, 3});
    REQUIRE(inspected.qualityAnalysis(state.reportContext()).at("shirt-blue").receivedUnits == 1);
    const auto completed =
        SalesReport::buildFinancialSummary(TimeWindow(20, 26), state.reportContext());
    REQUIRE(completed.captured() == Money{});
    REQUIRE(completed.refunded() == Money(34));
    REQUIRE(completed.deliveryRevenue() == Money{});
    REQUIRE(completed.deliveredNetGoods() == Money(66));
    REQUIRE(completed.returnRate() == ExactRatio{1, 3});
    REQUIRE(completed.averageOrderValue(state.reportContext()) == Money(66));
    REQUIRE(completed.skuStatistics().at("shirt-blue").deliveredUnits == 3);
    REQUIRE(completed.skuStatistics().at("shirt-blue").returnedUnits == 1);
    const auto all = SalesReport::buildFinancialSummary(TimeWindow(0, 30), state.reportContext());
    REQUIRE(all.captured().checkedSubtract(all.refunded()) == Money(76));
    REQUIRE(all.deliveredNetGoods() == Money(66));
    REQUIRE(all.reconcile(state.reportContext()).empty());
    const auto quality = all.qualityAnalysis(state.reportContext()).at("shirt-blue");
    REQUIRE(quality.claims == 1);
    REQUIRE(quality.acceptedDefectiveUnits == 1);
    REQUIRE(quality.restockUnits == 0);
    REQUIRE(quality.declinedUnits == 0);
    REQUIRE(quality.claimsByReason.at(ReturnReason::Defective) == 1);
    REQUIRE_THROWS_AS(SalesReport::buildFinancialSummary(TimeWindow(0, 31), state.reportContext()),
                      ReportRuleException);
}

TEST_CASE("Cancelled payment cash can be negative in the refund period without fabricated delivery "
          "revenue") {
    auto state = test::store();
    const auto id = test::place(state);
    test::pay(state, id);
    state.advanceClock(10);
    OrderProcessing(state).cancel("alice", id, "cancel");
    state.advanceClock(1);
    const auto report =
        SalesReport::buildFinancialSummary(TimeWindow(20, 21), state.reportContext());
    REQUIRE(report.captured() == Money{});
    REQUIRE(report.refunded() == Money(110));
    REQUIRE(report.deliveryRevenue() == Money(-10));
    REQUIRE(report.deliveredNetGoods() == Money{});
    REQUIRE_FALSE(report.returnRate());
    REQUIRE_FALSE(report.averageOrderValue(state.reportContext()));
    REQUIRE(report.reconcile(state.reportContext()).empty());
    REQUIRE(SalesReport::buildFinancialSummary(TimeWindow(0, 15), state.reportContext())
                .reconcile(state.reportContext())
                .empty());
}

TEST_CASE("ABC handles exact thresholds zero revenue and deterministic equal contributions") {
    const ProductVariant a("a", "A", {}, 1, {1, 1, 1}), b("b", "B", {}, 1, {1, 1, 1}),
        c("c", "C", {}, 1, {1, 1, 1}), z("z", "Z", {}, 1, {1, 1, 1});
    const PriceQuote quote("alice",
                           {QuoteLine(a, "A", 1, Money(80), Money{}, true),
                            QuoteLine(b, "B", 1, Money(15), Money{}, true),
                            QuoteLine(c, "C", 1, Money(5), Money{}, true),
                            QuoteLine(z, "Z", 1, Money(1), Money(1), true)},
                           test::address(), "standard", Money{},
                           {DiscountKind::Promotion, "free-z", std::nullopt}, 0, 100,
                           ReturnPolicy{});
    Order order("order", quote, "reservation", 0, 100, "place");
    order.confirmPayment(1, "pay", "p");
    order.beginFulfillment(2, "start", "f");
    order.markShipped(3, "ship", "f");
    order.markDelivered(4, "deliver", "f");
    OrderBook orders;
    orders.add(order);
    PaymentBook payments;
    const std::map<std::string, ReturnRequest> returns;
    const std::map<std::string, ReturnInspection> inspections;
    const std::map<std::string, Customer> customers{{"alice", test::customer()}};
    const ReportContext context(orders, payments, returns, inspections, customers, 10);
    const auto report = SalesReport::buildFinancialSummary(TimeWindow(0, 10), context);
    const auto abc = report.abcAnalysis();
    REQUIRE(abc.size() == 4);
    REQUIRE(abc[0].sku == "a");
    REQUIRE(abc[0].group == 'A');
    REQUIRE(abc[0].cumulativeRevenue == Money(80));
    REQUIRE(abc[1].group == 'B');
    REQUIRE(abc[2].group == 'C');
    REQUIRE(abc[3].group == 'C');
    REQUIRE(abc[3].netRevenue == Money{});
    REQUIRE_THROWS_AS(report.abcAnalysis(9500, 8000), ReportRuleException);
    REQUIRE(SalesReport::buildFinancialSummary(TimeWindow(5, 10), context).abcAnalysis().empty());
}

TEST_CASE(
    "RFM and repeat purchase metrics use explicit boundaries and distinct receipt semantics") {
    auto state = test::store();
    const auto first = deliveredPurchase(state, "alice", "first");
    state.advanceClock(1);
    deliveredPurchase(state, "alice", "second");
    state.advanceClock(9);
    auto report = SalesReport::buildFinancialSummary(TimeWindow(0, 20), state.reportContext());
    REQUIRE(report.repeatPurchaseRate(state.reportContext()) == ExactRatio{1, 1});
    const RfmRules rules{10, 20, 2, 3, Money(40), Money(50)};
    auto segments = report.rfmSegments(state.reportContext(), rules);
    REQUIRE(segments.at("alice").segment == "322");
    REQUIRE(segments.at("alice").frequency == 2);
    REQUIRE(segments.at("alice").recencySeconds == 9);
    REQUIRE(segments.at("alice").monetary == Money(40));
    REQUIRE(segments.at("bob").segment == "no-purchases");
    REQUIRE_FALSE(segments.at("bob").recencySeconds);
    REQUIRE(report.rfmSegments(state.reportContext(), RfmRules{9, 20, 1, 2, Money(20), Money(40)})
                .at("alice")
                .segment == "333");
    returnUnits(state, first, 1, "first");
    state.advanceClock(1);
    report = SalesReport::buildFinancialSummary(TimeWindow(0, 21), state.reportContext());
    REQUIRE(report.repeatPurchaseRate(state.reportContext()) == ExactRatio{0, 1});
    REQUIRE(report.averageOrderValue(state.reportContext()) == Money(10));
    REQUIRE(report.rfmSegments(state.reportContext(), rules).at("alice").frequency == 2);
    REQUIRE(report.rfmSegments(state.reportContext(), rules).at("alice").monetary == Money(20));
    REQUIRE_THROWS_AS(
        report.rfmSegments(state.reportContext(), RfmRules{20, 10, 2, 3, Money(1), Money(2)}),
        ReportRuleException);
}

TEST_CASE("Promotion reporting reconstructs old unpaid holds even after later extension and "
          "cancellation") {
    auto state = test::store(true);
    const auto id = test::place(state);
    OrderProcessing process(state);
    state.advanceClock(2);
    process.extendPaymentDeadline("alice", id, "extend");
    state.advanceClock(39);
    process.cancel("alice", id, "cancel");
    REQUIRE(state.coupons().heldUses().empty());
    const auto historical =
        SalesReport::buildFinancialSummary(TimeWindow(0, 50), state.reportContext());
    const auto usage =
        historical.promotionUsageAnalysis(state.reportContext()).at({DiscountKind::Coupon, "sale"});
    REQUIRE(usage.orders == 1);
    REQUIRE(usage.paidOrders == 0);
    REQUIRE(usage.heldCoupons == 1);
    REQUIRE(usage.goodsDiscount == Money(20));
    REQUIRE(usage.returnedGoods == Money{});
    const auto expiredWithoutExtension =
        SalesReport::buildFinancialSummary(TimeWindow(0, 11), state.reportContext());
    REQUIRE(expiredWithoutExtension.promotionUsageAnalysis(state.reportContext())
                .at({DiscountKind::Coupon, "sale"})
                .heldCoupons == 1);
    state.advanceClock(1);
    REQUIRE(SalesReport::buildFinancialSummary(TimeWindow(0, 52), state.reportContext())
                .promotionUsageAnalysis(state.reportContext())
                .at({DiscountKind::Coupon, "sale"})
                .heldCoupons == 0);
}

TEST_CASE("Reconciliation reports mismatched snapshots and orphan financial documents rather than "
          "silently accepting them") {
    OrderBook emptyOrders;
    PaymentBook payments;
    const std::map<std::string, ReturnRequest> returns;
    const std::map<std::string, ReturnInspection> inspections;
    const std::map<std::string, Customer> customers;
    const ReportContext empty(emptyOrders, payments, returns, inspections, customers, 100);
    const auto report = SalesReport::buildFinancialSummary(TimeWindow(0, 100), empty);
    REQUIRE(report.reconcile(empty).empty());
    REQUIRE_FALSE(report.repeatPurchaseRate(empty));
    OrderBook orders;
    orders.add(test::deliveredOrder());
    REQUIRE_FALSE(
        report.reconcile({orders, payments, returns, inspections, customers, 100}).empty());
    payments.recordAttempt("a", "p", test::order(), Money(110), PaymentMethod::SimulatedCard, 11,
                           true, "", "pay");
    REQUIRE_FALSE(
        report.reconcile({emptyOrders, payments, returns, inspections, customers, 100}).empty());
    REQUIRE_FALSE(
        report.reconcile({orders, payments, returns, inspections, customers, 100}).empty());
    payments.refund("r", "refund", "p", std::nullopt, Money(110), 12);
    REQUIRE_FALSE(
        report.reconcile({orders, payments, returns, inspections, customers, 100}).empty());
}
