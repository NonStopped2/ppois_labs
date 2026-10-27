#include "../BranchFixtures.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("MailBag checks capacity direction metadata and sealed snapshots", "[mail-bag]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto target = test::branch("B", "23");
    MailBag bag(" bag ", 20, " B ");
    REQUIRE(bag.id() == "bag");
    REQUIRE(bag.maxWeightGrams() == 20);
    REQUIRE(bag.destinationBranchId() == "B");
    REQUIRE_FALSE(bag.sealedAt());
    REQUIRE(bag.sealId().empty());
    bag.add(registry.at("BY-1"), target, test::sortingPlan());
    REQUIRE(bag.totalWeightGrams() == 20);
    REQUIRE(bag.shipmentIds().at("BY-1") == 20);
    const auto before = bag;
    REQUIRE_THROWS_AS(bag.add(registry.at("BY-1"), target, test::sortingPlan()),
                      InvalidShipmentStateException);
    REQUIRE(bag == before);
    auto copy = bag;
    copy.remove(" BY-1 ");
    REQUIRE(copy.totalWeightGrams() == 0);
    REQUIRE(copy.shipmentIds().empty());
    REQUIRE(copy != bag);
    REQUIRE_THROWS_AS(copy.remove("BY-1"), ShipmentNotFoundException);
    REQUIRE_THROWS_AS(copy.seal("s", test::moment(5), registry), InvalidShipmentStateException);
    bag.seal(" seal ", test::moment(5), registry);
    REQUIRE(bag.sealId() == "seal");
    REQUIRE(bag.sealedAt() == test::moment(5));
    REQUIRE_THROWS_AS(bag.add(registry.at("BY-1"), target, test::sortingPlan()),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(bag.remove("BY-1"), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(bag.seal("other", test::moment(6), registry), InvalidShipmentStateException);
}
TEST_CASE("MailBag rejects bad direction chronology and overload without state changes",
          "[mail-bag]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto target = test::branch("B", "23");
    MailBag bag("bag", 19, "B");
    const auto before = bag;
    REQUIRE_THROWS_AS(bag.add(registry.at("BY-1"), test::branch("C", "24"), test::sortingPlan()),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(bag.add(registry.at("BY-1"), target, test::sortingPlan("A", "C")),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(bag.add(registry.at("BY-1"), target, test::sortingPlan()),
                      ShipmentLimitExceededException);
    REQUIRE(bag == before);
    REQUIRE_THROWS_AS(MailBag("b", 0, "B"), std::invalid_argument);
    MailBag enough("b", 20, "B");
    enough.add(registry.at("BY-1"), target, test::sortingPlan());
    REQUIRE_THROWS_AS(enough.seal("s", test::moment(0), registry), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(enough.seal(" ", test::moment(10), registry), std::invalid_argument);
    test::advance(registry.at("BY-1"), TrackingEventKind::Lost);
    REQUIRE_THROWS_AS(enough.seal("s", test::moment(10), registry), InvalidShipmentStateException);
    REQUIRE_FALSE(enough.sealedAt());
}
TEST_CASE("MailBag supports extreme aggregate mass without intermediate overflow", "[mail-bag]") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    ShipmentRegistry registry("BY");
    const auto large = test::registerCargo(registry, maximum - 1);
    const auto small = test::registerCargo(registry, 1);
    const auto excess = test::registerCargo(registry, 1);
    for (const auto& id : {large, small, excess}) {
        test::advance(registry.at(id), TrackingEventKind::Accepted);
    }
    const auto target = test::branch("B", "23");
    MailBag bag("bag", maximum, "B");
    bag.add(registry.at(large), target, test::sortingPlan());
    bag.add(registry.at(small), target, test::sortingPlan());
    REQUIRE(bag.totalWeightGrams() == maximum);
    const auto before = bag;
    REQUIRE_THROWS_AS(bag.add(registry.at(excess), target, test::sortingPlan()),
                      ShipmentLimitExceededException);
    REQUIRE(bag == before);
    bag.remove(large);
    REQUIRE(bag.totalWeightGrams() == 1);
}
