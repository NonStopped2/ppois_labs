#include "../DeliveryFixtures.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"
#include "post/exceptions/StorageCapacityException.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace post;
TEST_CASE("Courier rechecks capacity when a copied route replaces the reserved plan", "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier(500);
    const auto light = test::acceptCourierParcel(registry, branch),
               heavy = test::acceptCourierParcel(registry, branch, test::deliveryPolicy(), 600);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("t", registry, branch, light, courier), route);
    DeliveryRoute replacement("r", "courier");
    replacement.addTask(test::deliveryTask("t", registry, branch, heavy, courier));
    route = replacement;
    const auto before = branch;
    REQUIRE_THROWS_AS(courier.startRoute(route, branch, registry, test::moment(20)),
                      ShipmentLimitExceededException);
    REQUIRE(branch == before);
    REQUIRE(route == replacement);
    REQUIRE(registry.at(heavy).status() == ShipmentStatus::AwaitingPickup);
}
TEST_CASE("Courier return rolls back every parcel when the last one has no warehouse capacity",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch(2);
    auto courier = test::courier();
    const auto first = test::acceptCourierParcel(registry, branch),
               second = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("one", registry, branch, first, courier), route);
    courier.assignTask(test::deliveryTask("two", registry, branch, second, courier), route);
    courier.startRoute(route, branch, registry, test::moment(20));
    route.recordAttempt("one", registry, courier, branch,
                        DeliveryAttempt("f1", test::moment(30), DeliveryOutcome::Failed,
                                        DeliveryFailureReason::RecipientAbsent, "Нет дома"),
                        test::deliveryPolicy());
    route.recordAttempt("two", registry, courier, branch,
                        DeliveryAttempt("f2", test::moment(31), DeliveryOutcome::Failed,
                                        DeliveryFailureReason::RecipientAbsent, "Нет дома"),
                        test::deliveryPolicy());
    const auto third = test::acceptCourierParcel(registry, branch);
    const auto before = branch;
    const auto routeBefore = route;
    const auto courierBefore = courier;
    const auto history = registry.at(first).history();
    REQUIRE_THROWS_AS(
        courier.finishRoute(route, branch, registry, test::deliveryPolicy(), test::moment(40)),
        StorageCapacityException);
    REQUIRE(branch == before);
    REQUIRE(route == routeBefore);
    REQUIRE(courier == courierBefore);
    REQUIRE(registry.at(first).history() == history);
    REQUIRE_THROWS_AS(branch.storageEntry(first), ShipmentNotFoundException);
    branch.issueShipment(registry, third, "worker", registry.at(third).recipient(),
                         test::document(), test::moment(35));
    courier.finishRoute(route, branch, registry, test::deliveryPolicy(), test::moment(40));
    REQUIRE(branch.storageEntry(first).shipmentId() == first);
    REQUIRE(branch.storageEntry(second).shipmentId() == second);
}
TEST_CASE("Courier rejects inconsistent assignment sets expired windows and repeated starts",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto first = test::acceptCourierParcel(registry, branch),
               second = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("one", registry, branch, first, courier), route);
    route.addTask(test::deliveryTask("two", registry, branch, second, courier));
    const auto before = route;
    const auto branchBefore = branch;
    REQUIRE_THROWS_AS(courier.startRoute(route, branch, registry, test::moment(20)),
                      InvalidShipmentStateException);
    REQUIRE(route == before);
    REQUIRE(branch == branchBefore);
    auto otherCourier = test::courier();
    DeliveryRoute other("other", "courier");
    otherCourier.assignTask(test::deliveryTask("t", registry, branch, first, otherCourier), other);
    REQUIRE_THROWS_AS(otherCourier.startRoute(other, branch, registry, test::moment(101)),
                      InvalidShipmentStateException);
    otherCourier.startRoute(other, branch, registry, test::moment(20));
    REQUIRE_THROWS_AS(otherCourier.startRoute(other, branch, registry, test::moment(21)),
                      InvalidShipmentStateException);
}
TEST_CASE("Courier reserves one route and enforces cumulative capacity", "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier(500);
    const auto first = test::acceptCourierParcel(registry, branch),
               second = test::acceptCourierParcel(registry, branch);
    REQUIRE(courier.employee().id() == "courier");
    REQUIRE(courier.vehicleId() == "bike");
    REQUIRE(courier.maxLoadGrams() == 500);
    REQUIRE_FALSE(courier.activeRouteId());
    DeliveryRoute route("route", "courier");
    const auto one = test::deliveryTask("one", registry, branch, first, courier);
    courier.assignTask(one, route);
    REQUIRE(courier.assignedTaskIds().contains("one"));
    REQUIRE(courier.activeRouteId() == "route");
    const auto before = courier;
    const auto routeBefore = route;
    REQUIRE_THROWS_AS(courier.assignTask(one, route), DeliveryAuthorizationException);
    const auto two = test::deliveryTask("two", registry, branch, second, courier);
    DeliveryRoute overfilled("overfilled", "courier");
    overfilled.addTask(one);
    overfilled.addTask(two);
    auto tiny = test::courier(500);
    REQUIRE_THROWS_AS(tiny.assignTask(one, overfilled), ShipmentLimitExceededException);
    REQUIRE_THROWS_AS(DeliveryTask("bad", registry.at(first), branch.storageEntry(second)),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(courier.assignTask(two, route), ShipmentLimitExceededException);
    DeliveryRoute other("other", "courier");
    REQUIRE_THROWS_AS(courier.assignTask(two, other), DeliveryAuthorizationException);
    REQUIRE(courier == before);
    REQUIRE(route == routeBefore);
    REQUIRE_THROWS_AS(Courier(test::employee(), "bike", 100), DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(test::courier(0), DeliveryAuthorizationException);
}
TEST_CASE("Courier starts physical delivery and returns failed parcels before finishing",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("t", registry, branch, id, courier), route);
    REQUIRE_THROWS_AS(
        courier.finishRoute(route, branch, registry, test::deliveryPolicy(), test::moment(20)),
        InvalidShipmentStateException);
    courier.startRoute(route, branch, registry, test::moment(20));
    REQUIRE(registry.at(id).status() == ShipmentStatus::OutForDelivery);
    REQUIRE_THROWS_AS(branch.storageEntry(id), ShipmentNotFoundException);
    const auto before = route;
    const auto branchBefore = branch;
    REQUIRE_THROWS_AS(
        courier.finishRoute(route, branch, registry, test::deliveryPolicy(), test::moment(21)),
        InvalidShipmentStateException);
    REQUIRE(route == before);
    REQUIRE(branch == branchBefore);
    route.recordAttempt("t", registry, courier, branch,
                        DeliveryAttempt("failed", test::moment(30), DeliveryOutcome::Failed,
                                        DeliveryFailureReason::RecipientAbsent, "Нет дома"),
                        test::deliveryPolicy());
    REQUIRE(registry.at(id).status() == ShipmentStatus::OutForDelivery);
    REQUIRE(route.tasks().front().state() == DeliveryTaskState::Failed);
    courier.finishRoute(route, branch, registry, test::deliveryPolicy(), test::moment(40));
    REQUIRE(registry.at(id).status() == ShipmentStatus::AwaitingPickup);
    REQUIRE(branch.storageEntry(id).receivedAt() == test::moment(40));
    REQUIRE(route.tasks().front().state() == DeliveryTaskState::ReturnedToBranch);
    REQUIRE_FALSE(courier.activeRouteId());
    REQUIRE(courier.assignedTaskIds().empty());
    REQUIRE(route.finishedAt() == test::moment(40));
}
TEST_CASE("Courier route startup rolls back earlier parcels when later registry data is stale",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto first = test::acceptCourierParcel(registry, branch),
               second = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("one", registry, branch, first, courier), route);
    courier.assignTask(test::deliveryTask("two", registry, branch, second, courier), route);
    registry.at(second).applyTrackingEvent(
        TrackingEvent(3, TrackingEventKind::Information, test::moment(50), "A", "Будущий факт"));
    const auto before = branch;
    const auto routeBefore = route;
    const auto history = registry.at(first).history();
    REQUIRE_THROWS_AS(courier.startRoute(route, branch, registry, test::moment(20)),
                      InvalidShipmentStateException);
    REQUIRE(branch == before);
    REQUIRE(route == routeBefore);
    REQUIRE(registry.at(first).history() == history);
}
