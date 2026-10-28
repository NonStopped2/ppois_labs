#include "../DeliveryFixtures.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"
#include "post/exceptions/StorageCapacityException.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace post;
TEST_CASE("Courier cancels expired plans without moving cargo and can deliver on a new route",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("t", registry, branch, id, courier), route);
    const auto warehouse = branch;
    REQUIRE_THROWS_AS(courier.startRoute(route, branch, registry, test::moment(101)),
                      InvalidShipmentStateException);
    courier.cancelRoute(route, branch, registry, test::moment(101));
    REQUIRE(branch == warehouse);
    REQUIRE_FALSE(courier.activeRouteId());
    REQUIRE(courier.assignedTaskIds().empty());
    REQUIRE_FALSE(route.startedAt());
    REQUIRE(route.finishedAt() == test::moment(101));
    REQUIRE(route.tasks().front().state() == DeliveryTaskState::Cancelled);
    REQUIRE(route.nextPendingTask() == nullptr);
    REQUIRE(registry.at(id).status() == ShipmentStatus::AwaitingPickup);
    REQUIRE(registry.at(id).history().events().back().occurredAt() == test::moment(101));
    auto task = route.tasks().front();
    const auto before = task;
    REQUIRE_THROWS_AS(task.reschedule({test::moment(100), test::moment(110)}, courier,
                                      registry.at(id), test::deliveryPolicy()),
                      InvalidShipmentStateException);
    REQUIRE(task == before);
    task.reschedule({test::moment(102), test::moment(110)}, courier, registry.at(id),
                    test::deliveryPolicy());
    REQUIRE_THROWS_AS(route.addTask(task), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(route.reorder({"t"}), InvalidShipmentStateException);
    DeliveryRoute next("next", "courier");
    courier.assignTask(task, next);
    REQUIRE_THROWS_AS(courier.startRoute(route, branch, registry, test::moment(102)),
                      DeliveryAuthorizationException);
    courier.startRoute(next, branch, registry, test::moment(102));
    next.completeDelivery("t", registry, courier, branch,
                          DeliveryAttempt("done", test::moment(103), DeliveryOutcome::Delivered),
                          test::deliveryPolicy(), registry.at(id).recipient(), test::document());
    courier.finishRoute(next, branch, registry, test::deliveryPolicy(), test::moment(104));
    REQUIRE(registry.at(id).status() == ShipmentStatus::Delivered);
    REQUIRE(next.tasks().front().attempts().size() == 1);
}
TEST_CASE("Courier interrupts an expired active route without inventing a delivery attempt",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("t", registry, branch, id, courier), route);
    const auto deadline = route.tasks().front().storageDeadlineAt();
    courier.startRoute(route, branch, registry, test::moment(20));
    courier.interruptRoute(route, branch, registry, test::deliveryPolicy(), test::moment(101));
    REQUIRE_FALSE(courier.activeRouteId());
    REQUIRE(courier.assignedTaskIds().empty());
    REQUIRE(route.finishedAt() == test::moment(101));
    REQUIRE(route.tasks().front().state() == DeliveryTaskState::ReturnedToBranch);
    REQUIRE(route.tasks().front().attempts().empty());
    REQUIRE(route.tasks().front().storageDeadlineAt() == deadline);
    REQUIRE(branch.storageEntry(id).receivedAt() == test::moment(101));
    REQUIRE(registry.at(id).status() == ShipmentStatus::AwaitingPickup);
    DeliveryTask fresh("fresh", registry.at(id), branch.storageEntry(id));
    REQUIRE_THROWS_AS(fresh.schedule({test::moment(102), test::moment(110)}, courier,
                                     registry.at(id), test::deliveryPolicy()),
                      InvalidShipmentStateException);
    auto task = route.tasks().front();
    REQUIRE_THROWS_AS(task.reschedule({test::moment(102), deadline}, courier, registry.at(id),
                                      test::deliveryPolicy()),
                      InvalidShipmentStateException);
    task.reschedule({test::moment(102), test::moment(110)}, courier, registry.at(id),
                    test::deliveryPolicy());
    DeliveryRoute next("next", "courier");
    courier.assignTask(task, next);
    courier.startRoute(next, branch, registry, test::moment(102));
    next.completeDelivery("t", registry, courier, branch,
                          DeliveryAttempt("done", test::moment(103), DeliveryOutcome::Delivered),
                          test::deliveryPolicy(), registry.at(id).recipient(), test::document());
    courier.finishRoute(next, branch, registry, test::deliveryPolicy(), test::moment(104));
    REQUIRE(registry.at(id).status() == ShipmentStatus::Delivered);
    REQUIRE(next.tasks().front().attempts().size() == 1);
    REQUIRE(next.tasks().front().storageDeadlineAt() == deadline);
}
TEST_CASE("Courier interruption preserves delivered parcels and real nonretryable failures",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier(1500);
    DeliveryRoute route("r", "courier");
    for (const auto& key : {"one", "two", "three"}) {
        const auto id = test::acceptCourierParcel(registry, branch);
        courier.assignTask(test::deliveryTask(key, registry, branch, id, courier), route);
    }
    courier.startRoute(route, branch, registry, test::moment(20));
    route.completeDelivery("one", registry, courier, branch,
                           DeliveryAttempt("done", test::moment(30), DeliveryOutcome::Delivered),
                           test::deliveryPolicy(), registry.at("BY-1").recipient(),
                           test::document());
    route.recordAttempt("two", registry, courier, branch,
                        DeliveryAttempt("refused", test::moment(31), DeliveryOutcome::Failed,
                                        DeliveryFailureReason::Refused, "Отказ"),
                        test::deliveryPolicy());
    const auto delivered = registry.at("BY-1").history();
    courier.interruptRoute(route, branch, registry, test::deliveryPolicy(), test::moment(101));
    REQUIRE(registry.at("BY-1").history() == delivered);
    REQUIRE(route.tasks()[0].state() == DeliveryTaskState::Delivered);
    REQUIRE(route.tasks()[1].attempts().size() == 1);
    REQUIRE(route.tasks()[2].attempts().empty());
    REQUIRE(branch.storageEntry("BY-2").shipmentId() == "BY-2");
    REQUIRE(branch.storageEntry("BY-3").shipmentId() == "BY-3");
    auto refused = route.tasks()[1];
    REQUIRE_THROWS_AS(refused.reschedule({test::moment(102), test::moment(110)}, courier,
                                         registry.at("BY-2"), test::deliveryPolicy()),
                      InvalidShipmentStateException);
}
TEST_CASE("Courier interruption rolls back all returns if warehouse capacity is insufficient",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch(2);
    auto courier = test::courier();
    DeliveryRoute route("r", "courier");
    for (const auto& key : {"one", "two"}) {
        const auto id = test::acceptCourierParcel(registry, branch);
        courier.assignTask(test::deliveryTask(key, registry, branch, id, courier), route);
    }
    courier.startRoute(route, branch, registry, test::moment(20));
    const auto filler = test::acceptCourierParcel(registry, branch);
    const auto warehouse = branch;
    const auto actor = courier;
    const auto plan = route;
    const auto first = registry.at("BY-1").history(), second = registry.at("BY-2").history();
    REQUIRE_THROWS_AS(
        courier.interruptRoute(route, branch, registry, test::deliveryPolicy(), test::moment(101)),
        StorageCapacityException);
    REQUIRE(branch == warehouse);
    REQUIRE(courier == actor);
    REQUIRE(route == plan);
    REQUIRE(registry.at("BY-1").history() == first);
    REQUIRE(registry.at("BY-2").history() == second);
    branch.issueShipment(registry, filler, "worker", registry.at(filler).recipient(),
                         test::document(), test::moment(101));
    courier.interruptRoute(route, branch, registry, test::deliveryPolicy(), test::moment(101));
    REQUIRE_FALSE(courier.activeRouteId());
    REQUIRE(branch.storageEntry("BY-1").shipmentId() == "BY-1");
    REQUIRE(branch.storageEntry("BY-2").shipmentId() == "BY-2");
}
TEST_CASE("Courier cancellation rejects missing authority and rolls back a late history error",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    DeliveryRoute route("r", "courier");
    for (const auto& key : {"one", "two"}) {
        const auto id = test::acceptCourierParcel(registry, branch);
        courier.assignTask(test::deliveryTask(key, registry, branch, id, courier), route);
    }
    registry.at("BY-2").applyTrackingEvent(
        TrackingEvent(3, TrackingEventKind::Information, test::moment(120), "A", "Будущий факт"));
    const auto plan = route;
    const auto actor = courier;
    const auto first = registry.at("BY-1").history();
    REQUIRE_THROWS_AS(courier.cancelRoute(route, branch, registry, test::moment(101)),
                      InvalidShipmentStateException);
    REQUIRE(route == plan);
    REQUIRE(courier == actor);
    REQUIRE(registry.at("BY-1").history() == first);
    branch.addShiftBreak("courier-shift", test::moment(120), test::moment(130));
    REQUIRE_THROWS_AS(courier.cancelRoute(route, branch, registry, test::moment(120)),
                      DeliveryAuthorizationException);
    REQUIRE(route == plan);
    courier.cancelRoute(route, branch, registry, test::moment(130));
    REQUIRE_FALSE(courier.activeRouteId());
    REQUIRE_THROWS_AS(courier.cancelRoute(route, branch, registry, test::moment(130)),
                      DeliveryAuthorizationException);
}
TEST_CASE("Courier interruption respects route phase branch authority and event chronology",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("t", registry, branch, id, courier), route);
    REQUIRE_THROWS_AS(
        courier.interruptRoute(route, branch, registry, test::deliveryPolicy(), test::moment(20)),
        InvalidShipmentStateException);
    courier.startRoute(route, branch, registry, test::moment(20));
    REQUIRE_THROWS_AS(courier.cancelRoute(route, branch, registry, test::moment(21)),
                      InvalidShipmentStateException);
    const auto plan = route;
    const auto warehouse = branch;
    registry.at(id).applyTrackingEvent(
        TrackingEvent(4, TrackingEventKind::Information, test::moment(120), "A", "Будущий факт"));
    REQUIRE_THROWS_AS(
        courier.interruptRoute(route, branch, registry, test::deliveryPolicy(), test::moment(19)),
        InvalidShipmentStateException);
    REQUIRE_THROWS_AS(
        courier.interruptRoute(route, branch, registry, test::deliveryPolicy(), test::moment(101)),
        InvalidShipmentStateException);
    const auto foreignActor = test::employee("courier", "B", EmployeeRole::Courier);
    WorkShift foreignShift("foreign", "courier", "B", test::moment(0), test::moment(10000));
    foreignShift.start(foreignActor, "B", test::moment(0));
    PostalBranch otherBranch("B", "Другое отделение",
                             PostalAddress("230000", "Город", "Улица", "1"), {test::storageArea()},
                             {foreignActor}, {foreignShift}, {"23"});
    REQUIRE_THROWS_AS(courier.interruptRoute(route, otherBranch, registry, test::deliveryPolicy(),
                                             test::moment(120)),
                      DeliveryAuthorizationException);
    REQUIRE(route == plan);
    REQUIRE(branch == warehouse);
    courier.interruptRoute(route, branch, registry, test::deliveryPolicy(), test::moment(120));
    REQUIRE_FALSE(courier.activeRouteId());
}
TEST_CASE("Courier interruption does not reset earlier failures or their policy limits",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("t", registry, branch, id, courier), route);
    courier.startRoute(route, branch, registry, test::moment(20));
    route.recordAttempt("t", registry, courier, branch,
                        DeliveryAttempt("f", test::moment(30), DeliveryOutcome::Failed,
                                        DeliveryFailureReason::RecipientAbsent, "Нет дома"),
                        test::deliveryPolicy());
    courier.finishRoute(route, branch, registry, test::deliveryPolicy(), test::moment(40));
    auto task = route.tasks().front();
    task.reschedule({test::moment(50), test::moment(100)}, courier, registry.at(id),
                    test::deliveryPolicy());
    DeliveryRoute next("next", "courier");
    courier.assignTask(task, next);
    courier.startRoute(next, branch, registry, test::moment(50));
    courier.interruptRoute(next, branch, registry, test::deliveryPolicy(), test::moment(101));
    auto returned = next.tasks().front();
    REQUIRE(returned.attempts().size() == 1);
    REQUIRE(returned.storageDeadlineAt() == route.tasks().front().storageDeadlineAt());
    REQUIRE_THROWS_AS(returned.reschedule({test::moment(102), test::moment(110)}, courier,
                                          registry.at(id), test::deliveryPolicy(1)),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(returned.reschedule({test::moment(102), test::moment(110)}, courier,
                                          registry.at(id), test::deliveryPolicy(2, 1)),
                      InvalidShipmentStateException);
}
TEST_CASE("Courier can physically return overdue cargo without granting a new delivery cycle",
          "[courier]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("t", registry, branch, id, courier), route);
    courier.startRoute(route, branch, registry, test::moment(20));
    const auto deadline = route.tasks().front().storageDeadlineAt();
    WorkShift late("late", "courier", "A", test::moment(10000), deadline + std::chrono::hours{1});
    branch.addShift(late);
    branch.startShift("late", test::moment(10000));
    const auto now = deadline + std::chrono::seconds{1};
    courier.interruptRoute(route, branch, registry, test::deliveryPolicy(), now);
    REQUIRE(branch.storageEntry(id).shipmentId() == id);
    auto task = route.tasks().front();
    REQUIRE(task.attempts().empty());
    REQUIRE(task.storageDeadlineAt() == deadline);
    REQUIRE_THROWS_AS(task.reschedule({now, now}, courier, registry.at(id), test::deliveryPolicy()),
                      InvalidShipmentStateException);
    DeliveryTask fresh("fresh", registry.at(id), branch.storageEntry(id));
    REQUIRE_THROWS_AS(fresh.schedule({now, now}, courier, registry.at(id), test::deliveryPolicy()),
                      InvalidShipmentStateException);
}
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
