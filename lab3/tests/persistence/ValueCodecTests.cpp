#include "../StoreFixtures.hpp"
#include "SnapshotJson.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace shop;
using namespace shop::snapshot;

TEST_CASE("JSON values round trip policies physical facts quotes and typed history") {
    auto state = test::store(true);
    REQUIRE(snapshot::address(value(test::address())) == test::address());
    REQUIRE(contacts(value(test::customer().contacts())) == test::customer().contacts());
    REQUIRE(window(value(TimeWindow(1, 2))) == TimeWindow(1, 2));
    REQUIRE(attribute(value(state.catalog().product("shirt").attributes().front())) ==
            state.catalog().product("shirt").attributes().front());
    REQUIRE(variant(value(state.catalog().variant("shirt-blue"))) ==
            state.catalog().variant("shirt-blue"));
    REQUIRE(returnsPolicy(value(ReturnPolicy{})) == ReturnPolicy{});
    REQUIRE(deliveryPolicy(value(state.deliveryPolicy())) == state.deliveryPolicy());
    REQUIRE(loyaltyPolicy(value(LoyaltyPolicy{})) == LoyaltyPolicy{});
    REQUIRE(pricingPolicy(value(PricingPolicy{})) == PricingPolicy{});
    REQUIRE(promotion(value(state.promotions().at("sale"))) == state.promotions().at("sale"));
    REQUIRE(coupon(value(state.coupons().coupon("SAVE"))) == state.coupons().coupon("SAVE"));
    const auto quote = Checkout(state).preview("alice", test::address(), "standard");
    REQUIRE(snapshot::quote(value(quote)) == quote);
    const InspectionResult result("shirt-blue", 2, 1, 1, ItemCondition::Opened);
    REQUIRE(inspectionResult(value(result)) == result);
    const DeliveryTerms terms{test::address(), "standard", Money(10)};
    REQUIRE(deliveryTerms(value(terms)) == terms);
    const std::vector<OrderEvent> events{
        {1, OrderEventKind::Created, "place", "r", "Начало", InitialOrderTerms{10, terms}},
        {2, OrderEventKind::DeadlineExtended, "extend", "", "", DeadlineChange{10, 20}},
        {3, OrderEventKind::DeliveryChanged, "change", "", "",
         DeliveryChange{terms, {test::address(), "express", Money(20)}}},
        {4, OrderEventKind::PaymentConfirmed, "pay", "p"}};
    for (const auto& entry : events) {
        REQUIRE(event(value(entry)) == entry);
    }
}

TEST_CASE("JSON scalar parsing never coerces floats booleans null or oversized integers") {
    REQUIRE(integer(Json(std::numeric_limits<std::int64_t>::max())) ==
            std::numeric_limits<std::int64_t>::max());
    REQUIRE(integer(Json(-1)) == -1);
    REQUIRE(integer(Json(0U)) == 0);
    REQUIRE_THROWS_AS(integer(Json(1.0)), SnapshotException);
    REQUIRE_THROWS_AS(integer(Json(true)), SnapshotException);
    REQUIRE_THROWS_AS(integer(Json(std::numeric_limits<std::uint64_t>::max())), SnapshotException);
    REQUIRE_THROWS_AS(boolean(Json(1)), SnapshotException);
    REQUIRE_THROWS_AS(string(Json(nullptr)), SnapshotException);
    REQUIRE_THROWS_AS(array(Json::object()), SnapshotException);
    REQUIRE_THROWS_AS(object(Json::array()), SnapshotException);
    REQUIRE_THROWS_AS(window(Json::array({1})), SnapshotException);
    REQUIRE_THROWS_AS(stringSet(Json::array({"a", "a"})), SnapshotException);
    REQUIRE(quantities(Json{{"sku", 2}}).at("sku") == 2);
    REQUIRE(optionalString(nullptr) == std::nullopt);
    REQUIRE(nullable(std::nullopt).is_null());
    auto state = test::store();
    auto quote = Checkout(state).preview("alice", test::address(), "standard");
    auto data = value(quote);
    data["total"] = 111;
    REQUIRE_THROWS_AS(snapshot::quote(data), SnapshotException);
    data = value(quote.lines().front());
    data["net_total"] = 111;
    REQUIRE_THROWS_AS(quoteLine(data), SnapshotException);
    data = value(quote.lines().front());
    data["dimensions_mm"] = Json::array({1, 2});
    REQUIRE_THROWS_AS(quoteLine(data), SnapshotException);
    REQUIRE_THROWS_AS(enumeration(Json(99), DiscountKind::Loyalty), SnapshotException);
}
