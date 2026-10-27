#include "../StaffFixtures.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("SortingPlan groups by highest priority without changing shipment history",
          "[sorting-plan]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    const SortingRule broad("broad", {"22"}, "B", 1, {ShipmentKind::Letter});
    const SortingRule specific("specific", {"220"}, "C", 2, {ShipmentKind::Letter});
    const SortingPlan plan(" p ", " A ", {broad, specific}, 3);
    REQUIRE(plan.id() == "p");
    REQUIRE(plan.branchId() == "A");
    REQUIRE(plan.version() == 3);
    REQUIRE(plan.rules().size() == 2);
    REQUIRE_NOTHROW(plan.validateRules());
    const auto before = registry.at("BY-1").history();
    REQUIRE(plan.groupForDispatch({std::cref(registry.at("BY-1"))}).at("C") ==
            std::vector<std::string>{"BY-1"});
    REQUIRE(registry.at("BY-1").history() == before);
    REQUIRE(plan.groupForDispatch({}).empty());
    REQUIRE(plan == plan);
    REQUIRE(plan != test::sortingPlan());
    const SortingPlan sameDirection(
        "same", "A", {broad, SortingRule("same", {"220"}, "B", 1, {ShipmentKind::Letter})}, 1);
    REQUIRE(sameDirection.groupForDispatch({std::cref(registry.at("BY-1"))}).contains("B"));
}
TEST_CASE("SortingPlan rejects duplicate inputs wrong location and missing rules",
          "[sorting-plan]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto plan = test::sortingPlan();
    REQUIRE_THROWS_AS(
        plan.groupForDispatch({std::cref(registry.at("BY-1")), std::cref(registry.at("BY-1"))}),
        InvalidShipmentStateException);
    REQUIRE_THROWS_AS(
        test::sortingPlan("B", "C").groupForDispatch({std::cref(registry.at("BY-1"))}),
        InvalidShipmentStateException);
    const auto unregistered = test::letter();
    REQUIRE_THROWS_AS(plan.groupForDispatch({std::cref(unregistered)}),
                      InvalidShipmentStateException);
    const SortingPlan uncovered("p", "A",
                                {SortingRule("r", {"00"}, "B", 1, {ShipmentKind::Letter})}, 1);
    REQUIRE_THROWS_AS(uncovered.groupForDispatch({std::cref(registry.at("BY-1"))}),
                      InvalidShipmentStateException);
}
TEST_CASE("SortingPlan rejects ambiguous rules duplicate keys and self direction",
          "[sorting-plan]") {
    const SortingRule rule("r", {"22"}, "B", 1, {ShipmentKind::Letter});
    REQUIRE_THROWS_AS(SortingPlan("p", "A", {}, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(SortingPlan("p", "A", {rule}, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(SortingPlan("p", "B", {rule}, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(SortingPlan("p", "A", {rule, rule}, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(
        SortingPlan("p", "A", {rule, SortingRule("x", {"220"}, "C", 1, {ShipmentKind::Letter})}, 1),
        std::invalid_argument);
}
