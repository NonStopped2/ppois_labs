#include "../StaffFixtures.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("SortingRule uses destination prefix and product and preserves its metadata",
          "[sorting-rule]") {
    const SortingRule rule(" r ", {" 22 "}, " B ", 10, {ShipmentKind::Letter});
    REQUIRE(rule.id() == "r");
    REQUIRE(rule.destinationPrefixes() == std::vector<std::string>{"22"});
    REQUIRE(rule.nextBranchId() == "B");
    REQUIRE(rule.priority() == 10);
    REQUIRE(rule.allowedShipmentKinds() == std::vector{ShipmentKind::Letter});
    REQUIRE(rule.matches(test::letter()));
    REQUIRE_FALSE(rule.matches(test::parcel()));
    REQUIRE_FALSE(SortingRule("x", {"23"}, "B", 1, {ShipmentKind::Letter}).matches(test::letter()));
    REQUIRE(rule == rule);
    REQUIRE(rule != SortingRule("x", {"22"}, "B", 1, {ShipmentKind::Letter}));
}
TEST_CASE("SortingRule conflicts require equal priority shared products and different directions",
          "[sorting-rule]") {
    const SortingRule rule("r", {"22"}, "B", 1, {ShipmentKind::Letter});
    REQUIRE(rule.conflictsWith(SortingRule("x", {"220"}, "C", 1, {ShipmentKind::Letter})));
    REQUIRE_FALSE(rule.conflictsWith(SortingRule("x", {"220"}, "B", 1, {ShipmentKind::Letter})));
    REQUIRE_FALSE(rule.conflictsWith(SortingRule("x", {"220"}, "C", 2, {ShipmentKind::Letter})));
    REQUIRE_FALSE(rule.conflictsWith(SortingRule("x", {"23"}, "C", 1, {ShipmentKind::Letter})));
    REQUIRE_FALSE(rule.conflictsWith(SortingRule("x", {"22"}, "C", 1, {ShipmentKind::Parcel})));
}
TEST_CASE("SortingRule changes effective destination during return without swapping customers",
          "[sorting-rule]") {
    const Letter prototype(test::customer("s", "220000"), test::customer("r", "230000"), 20,
                           DeliveryOptions{}, Dimensions(200, 100, 5), 1, "Документы");
    ShipmentRegistry registry("BY");
    const auto id = registry.registerShipment(prototype, 100, Dimensions(200, 100, 5), 10);
    auto& shipment = registry.at(id);
    test::advance(shipment, TrackingEventKind::Accepted);
    test::advance(shipment, TrackingEventKind::ReturnStarted);
    REQUIRE(SortingRule("back", {"22"}, "A", 1, {ShipmentKind::Letter}).matches(shipment));
    REQUIRE_FALSE(SortingRule("forward", {"23"}, "B", 1, {ShipmentKind::Letter}).matches(shipment));
    REQUIRE(shipment.recipient().id() == "r");
}
TEST_CASE("SortingRule rejects invalid prefix and product configuration", "[sorting-rule]") {
    for (const auto& prefixes : std::vector<std::vector<std::string>>{
             {}, {""}, {"2x"}, {"1234567"}, {"22", "220"}, {"22", "22"}}) {
        REQUIRE_THROWS_AS(SortingRule("r", prefixes, "B", 1, {ShipmentKind::Letter}),
                          std::invalid_argument);
    }
    REQUIRE_THROWS_AS(SortingRule("r", {"22"}, "B", -1, {ShipmentKind::Letter}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(SortingRule("r", {"22"}, "B", 1, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(SortingRule("r", {"22"}, "B", 1, {static_cast<ShipmentKind>(99)}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        SortingRule("r", {"22"}, "B", 1, {ShipmentKind::Letter, ShipmentKind::Letter}),
        std::invalid_argument);
}
