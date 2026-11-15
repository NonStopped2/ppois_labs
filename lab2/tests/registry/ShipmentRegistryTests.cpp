#include "../Fixtures.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <utility>

using namespace post;
using namespace std::chrono;
TEST_CASE("Registry assigns unique numbers without consuming caller prototypes", "[registry]") {
    ShipmentRegistry registry(" BY ", 7);
    const auto prototype = test::letter();
    const auto first = registry.registerShipment(prototype, 100, Dimensions(300, 200, 10), 5);
    REQUIRE(first == "BY-7");
    REQUIRE(test::registerLetter(registry) == "BY-8");
    REQUIRE(prototype.trackingNumber().empty());
    REQUIRE(registry.nextSequence() == 9);
    REQUIRE(registry.trackingPrefix() == "BY");
    REQUIRE(registry.activeCount() == 2);
    REQUIRE_THROWS_AS(
        registry.registerShipment(registry.at(first), 100, Dimensions(300, 200, 10), 5),
        InvalidShipmentStateException);
    REQUIRE_THROWS_AS(registry.registerShipment(prototype, 1, Dimensions(300, 200, 10), 5),
                      ShipmentLimitExceededException);
    REQUIRE(registry.nextSequence() == 9);
    REQUIRE_THROWS_AS(registry.at("absent"), ShipmentNotFoundException);
    REQUIRE_THROWS_AS(registry.at("bad id"), std::invalid_argument);
}
TEST_CASE("Registry preserves terminal counter and constructor boundaries", "[registry]") {
    REQUIRE_THROWS_AS(ShipmentRegistry(" "), std::invalid_argument);
    REQUIRE_THROWS_AS(ShipmentRegistry("X", 0), std::invalid_argument);
    ShipmentRegistry registry("X", std::numeric_limits<std::int64_t>::max());
    REQUIRE(test::registerLetter(registry) == "X-9223372036854775807");
    REQUIRE(registry.nextSequence() == 0);
    REQUIRE_THROWS_AS(test::registerLetter(registry), std::overflow_error);
    REQUIRE(registry.activeCount() == 1);
    auto cancelled = test::letter();
    cancelled.cancelBeforeAcceptance(sys_seconds{}, "Отмена");
    ShipmentRegistry other("Y");
    REQUIRE_THROWS_AS(other.registerShipment(cancelled, 100, Dimensions(300, 200, 10), 5),
                      InvalidShipmentStateException);
}
TEST_CASE("Registry archives only completed records strictly before cutoff", "[registry]") {
    auto registry = test::atStatus(ShipmentStatus::Cancelled);
    const auto& originalAddress = registry.at("BY-1");
    test::advance(registry.at("BY-1"), TrackingEventKind::Information);
    const auto active = test::registerLetter(registry);
    REQUIRE(registry.archiveCompletedBefore(sys_seconds{seconds{1}}) == 0);
    REQUIRE(registry.archiveCompletedBefore(sys_seconds{seconds{2}}) == 1);
    REQUIRE(&registry.at("BY-1") == &originalAddress);
    REQUIRE(registry.archivedCount() == 1);
    REQUIRE(registry.activeCount() == 1);
    REQUIRE(registry.at(active).status() == ShipmentStatus::Draft);
    REQUIRE(registry.findByStatus(ShipmentStatus::Cancelled).size() == 1);
    REQUIRE(registry.findByCustomer(test::customer("sender")).size() == 2);
    REQUIRE(registry.findByCustomer(test::customer("recipient")).size() == 2);
    REQUIRE(registry.findByCustomer(test::customer("unknown")).empty());
    REQUIRE_THROWS_AS(registry.archiveCompletedBefore(sys_seconds{seconds{-1}}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(registry.findByStatus(static_cast<ShipmentStatus>(99)),
                      std::invalid_argument);
}
TEST_CASE("Registry deep copies active and archived polymorphic objects", "[registry]") {
    auto original = test::atStatus(ShipmentStatus::Cancelled);
    REQUIRE(original.archiveCompletedBefore(sys_seconds{seconds{2}}) == 1);
    const auto parcelId =
        original.registerShipment(test::parcel(), 1000, Dimensions(500, 500, 500), 10);
    auto copy = original;
    REQUIRE(&copy.at(parcelId) != &original.at(parcelId));
    REQUIRE(dynamic_cast<const Parcel*>(&std::as_const(copy).at(parcelId)) != nullptr);
    copy.at(parcelId).revise(600, DeliveryOptions{});
    REQUIRE(original.at(parcelId).weightGrams() == 500);
    REQUIRE(copy.at(parcelId).weightGrams() == 600);
    ShipmentRegistry assigned("OTHER");
    assigned = original;
    assigned = assigned;
    REQUIRE(assigned.activeCount() == 1);
    REQUIRE(assigned.archivedCount() == 1);
    REQUIRE(assigned.at("BY-1").status() == ShipmentStatus::Cancelled);
    auto moved = std::move(assigned);
    REQUIRE(moved.at(parcelId).weightGrams() == 500);
    assigned = std::move(moved);
    REQUIRE(assigned.trackingPrefix() == "BY");
}
