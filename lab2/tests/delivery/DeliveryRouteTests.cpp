#include "../DeliveryFixtures.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace post;
TEST_CASE("DeliveryRoute rejects duplicate cargo foreign tasks and foreign live couriers",
          "[delivery-route]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    auto task = test::deliveryTask("t", registry, branch, id, courier);
    route.addTask(task);
    REQUIRE_THROWS_AS(route.addTask(task), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(route.addTask(test::deliveryTask("other-key", registry, branch, id, courier)),
                      InvalidShipmentStateException);
    DeliveryRoute foreign("foreign", "someone-else");
    REQUIRE_THROWS_AS(foreign.addTask(task), InvalidShipmentStateException);
    auto other = test::courier();
    DeliveryRoute active("active", "courier");
    courier.assignTask(task, active);
    courier.startRoute(active, branch, registry, test::moment(20));
    REQUIRE_THROWS_AS(
        active.recordAttempt("t", registry, other, branch,
                             DeliveryAttempt("f", test::moment(30), DeliveryOutcome::Failed,
                                             DeliveryFailureReason::Refused, "Отказ"),
                             test::deliveryPolicy()),
        InvalidShipmentStateException);
    REQUIRE_THROWS_AS(active.completeDelivery(
                          "t", registry, other, branch,
                          DeliveryAttempt("s", test::moment(30), DeliveryOutcome::Delivered),
                          test::deliveryPolicy(), registry.at(id).recipient(), test::document()),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(
        foreign.recordAttempt("t", registry, courier, branch,
                              DeliveryAttempt("f", test::moment(30), DeliveryOutcome::Failed,
                                              DeliveryFailureReason::Refused, "Отказ"),
                              test::deliveryPolicy()),
        InvalidShipmentStateException);
}
TEST_CASE("DeliveryRoute validates owner order permutations and read-only pending selection",
          "[delivery-route]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto first = test::acceptCourierParcel(registry, branch),
               second = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route(" route ", "courier");
    REQUIRE(route.id() == "route");
    REQUIRE(route.courierId() == "courier");
    REQUIRE_FALSE(route.startedAt());
    REQUIRE_FALSE(route.finishedAt());
    REQUIRE_FALSE(route.nextPendingTask());
    const auto one = test::deliveryTask("one", registry, branch, first, courier),
               two = test::deliveryTask("two", registry, branch, second, courier);
    courier.assignTask(one, route);
    courier.assignTask(two, route);
    REQUIRE(route.nextPendingTask()->id() == "one");
    REQUIRE(route.currentTaskIndex() == 0);
    const auto before = route;
    REQUIRE_THROWS_AS(route.reorder({"one"}), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(route.reorder({"one", "one"}), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(route.reorder({"one", "missing"}), InvalidShipmentStateException);
    REQUIRE(route == before);
    route.reorder({"two", "one"});
    REQUIRE(route.nextPendingTask()->id() == "two");
    REQUIRE(route != before);
    courier.startRoute(route, branch, registry, test::moment(20));
    REQUIRE_THROWS_AS(route.reorder({"one", "two"}), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(
        route.recordAttempt("one", registry, courier, branch,
                            DeliveryAttempt("f", test::moment(30), DeliveryOutcome::Failed,
                                            DeliveryFailureReason::Refused, "Отказ"),
                            test::deliveryPolicy()),
        InvalidShipmentStateException);
    REQUIRE_THROWS_AS(route.complete(test::moment(30)), InvalidShipmentStateException);
}
TEST_CASE("DeliveryRoute completes authorized deliveries in order and releases courier",
          "[delivery-route]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("t", registry, branch, id, courier), route);
    REQUIRE_THROWS_AS(
        route.recordAttempt("t", registry, courier, branch,
                            DeliveryAttempt("early", test::moment(20), DeliveryOutcome::Failed,
                                            DeliveryFailureReason::Refused, "Отказ"),
                            test::deliveryPolicy()),
        InvalidShipmentStateException);
    courier.startRoute(route, branch, registry, test::moment(20));
    route.completeDelivery("t", registry, courier, branch,
                           DeliveryAttempt("done", test::moment(30), DeliveryOutcome::Delivered),
                           test::deliveryPolicy(), registry.at(id).recipient(), test::document());
    REQUIRE_FALSE(route.nextPendingTask());
    REQUIRE(route.currentTaskIndex() == 1);
    REQUIRE(registry.at(id).status() == ShipmentStatus::Delivered);
    REQUIRE_THROWS_AS(route.complete(test::moment(29)), InvalidShipmentStateException);
    route.complete(test::moment(31));
    courier.finishRoute(route, branch, registry, test::deliveryPolicy(), test::moment(32));
    REQUIRE_FALSE(courier.activeRouteId());
    REQUIRE(route.finishedAt() == test::moment(31));
    REQUIRE_THROWS_AS(
        courier.finishRoute(route, branch, registry, test::deliveryPolicy(), test::moment(33)),
        PostalException);
}
