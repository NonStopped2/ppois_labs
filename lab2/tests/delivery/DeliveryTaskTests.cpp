#include "../DeliveryFixtures.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace post;
TEST_CASE("DeliveryTask requires current branch shift outside breaks for each physical attempt",
          "[delivery-task]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("t", registry, branch, id, courier), route);
    courier.startRoute(route, branch, registry, test::moment(20));
    branch.addShiftBreak("courier-shift", test::moment(30), test::moment(40));
    const auto before = route;
    const auto history = registry.at(id).history();
    REQUIRE_THROWS_AS(route.completeDelivery(
                          "t", registry, courier, branch,
                          DeliveryAttempt("done", test::moment(30), DeliveryOutcome::Delivered),
                          test::deliveryPolicy(), registry.at(id).recipient(), test::document()),
                      DeliveryAuthorizationException);
    REQUIRE(route == before);
    REQUIRE(registry.at(id).history() == history);
    route.completeDelivery("t", registry, courier, branch,
                           DeliveryAttempt("done", test::moment(40), DeliveryOutcome::Delivered),
                           test::deliveryPolicy(), registry.at(id).recipient(), test::document());
    REQUIRE(registry.at(id).status() == ShipmentStatus::Delivered);
}
TEST_CASE(
    "DeliveryTask rejects success through failure API wrong outcomes and out of window attempts",
    "[delivery-task]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("t", registry, branch, id, courier), route);
    courier.startRoute(route, branch, registry, test::moment(20));
    const auto before = route;
    REQUIRE_THROWS_AS(
        route.recordAttempt("t", registry, courier, branch,
                            DeliveryAttempt("done", test::moment(30), DeliveryOutcome::Delivered),
                            test::deliveryPolicy()),
        InvalidShipmentStateException);
    REQUIRE_THROWS_AS(route.completeDelivery(
                          "t", registry, courier, branch,
                          DeliveryAttempt("failed", test::moment(30), DeliveryOutcome::Failed,
                                          DeliveryFailureReason::Refused, "Отказ"),
                          test::deliveryPolicy(), registry.at(id).recipient(), test::document()),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(
        route.recordAttempt("t", registry, courier, branch,
                            DeliveryAttempt("f", test::moment(101), DeliveryOutcome::Failed,
                                            DeliveryFailureReason::Refused, "Отказ"),
                            test::deliveryPolicy()),
        InvalidShipmentStateException);
    REQUIRE(route == before);
}
TEST_CASE("DeliveryTask cannot erase accumulated failures by constructing a fresh task",
          "[delivery-task]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryRoute route("r", "courier");
    const auto original = test::deliveryTask("t", registry, branch, id, courier);
    courier.assignTask(original, route);
    courier.startRoute(route, branch, registry, test::moment(20));
    route.recordAttempt("t", registry, courier, branch,
                        DeliveryAttempt("failed", test::moment(30), DeliveryOutcome::Failed,
                                        DeliveryFailureReason::Refused, "Отказ"),
                        test::deliveryPolicy());
    courier.finishRoute(route, branch, registry, test::deliveryPolicy(), test::moment(40));
    DeliveryTask fresh("new", registry.at(id), branch.storageEntry(id));
    REQUIRE_THROWS_AS(fresh.schedule({test::moment(50), test::moment(100)}, courier,
                                     registry.at(id), test::deliveryPolicy()),
                      InvalidShipmentStateException);
    auto returned = route.tasks().front();
    REQUIRE_THROWS_AS(returned.reschedule({test::moment(50), test::moment(100)}, courier,
                                          registry.at(id), test::deliveryPolicy()),
                      InvalidShipmentStateException);
    DeliveryRoute stale("stale", "courier");
    courier.assignTask(original, stale);
    REQUIRE_THROWS_AS(courier.startRoute(stale, branch, registry, test::moment(50)),
                      InvalidShipmentStateException);
}
TEST_CASE("DeliveryTask snapshots posting data and rejects inconsistent windows",
          "[delivery-task]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    const auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    DeliveryTask task(" task ", registry.at(id), branch.storageEntry(id));
    REQUIRE(task.id() == "task");
    REQUIRE(task.shipmentId() == id);
    REQUIRE(task.recipient() == registry.at(id).recipient());
    REQUIRE(task.address() == registry.at(id).recipient().address());
    REQUIRE(task.weightGrams() == 500);
    REQUIRE(task.attempts().empty());
    REQUIRE(task.state() == DeliveryTaskState::Draft);
    REQUIRE(task.courierId().empty());
    REQUIRE(task.storageReceivedAt() == test::moment(15));
    REQUIRE(task.storageDeadlineAt() == branch.storageEntry(id).deadlineAt());
    const auto before = task;
    const auto other = test::acceptCourierParcel(registry, branch);
    REQUIRE_THROWS_AS(task.schedule({test::moment(20), test::moment(100)}, courier,
                                    registry.at(other), test::deliveryPolicy()),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(task.schedule({test::moment(30), test::moment(20)}, courier, registry.at(id),
                                    test::deliveryPolicy()),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(task.schedule({task.storageDeadlineAt(), task.storageDeadlineAt()}, courier,
                                    registry.at(id), test::deliveryPolicy()),
                      InvalidShipmentStateException);
    REQUIRE(task == before);
    task.schedule({test::moment(20), test::moment(100)}, courier, registry.at(id),
                  test::deliveryPolicy());
    REQUIRE(task.courierId() == "courier");
    REQUIRE(task.scheduledWindow().first == test::moment(20));
    REQUIRE_THROWS_AS(task.schedule({test::moment(20), test::moment(100)}, courier, registry.at(id),
                                    test::deliveryPolicy()),
                      InvalidShipmentStateException);
}
TEST_CASE(
    "DeliveryTask failed attempts can retry only after physical return and within original policy",
    "[delivery-task]") {
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
    auto pending = route.tasks().front();
    REQUIRE_THROWS_AS(pending.reschedule({test::moment(50), test::moment(100)}, courier,
                                         registry.at(id), test::deliveryPolicy()),
                      InvalidShipmentStateException);
    courier.finishRoute(route, branch, registry, test::deliveryPolicy(), test::moment(40));
    auto retry = route.tasks().front();
    const auto originalDeadline = retry.storageDeadlineAt();
    REQUIRE_THROWS_AS(retry.reschedule({test::moment(50), test::moment(100)}, courier,
                                       registry.at(id), test::deliveryPolicy(1)),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(retry.reschedule({test::moment(50), test::moment(100)}, courier,
                                       registry.at(id), test::deliveryPolicy(2, 1)),
                      InvalidShipmentStateException);
    retry.reschedule({test::moment(50), test::moment(100)}, courier, registry.at(id),
                     test::deliveryPolicy());
    REQUIRE(retry.attempts().size() == 1);
    REQUIRE(retry.storageDeadlineAt() == originalDeadline);
    DeliveryRoute next("next", "courier");
    courier.assignTask(retry, next);
    courier.startRoute(next, branch, registry, test::moment(50));
    next.completeDelivery("t", registry, courier, branch,
                          DeliveryAttempt("done", test::moment(60), DeliveryOutcome::Delivered),
                          test::deliveryPolicy(), registry.at(id).recipient(), test::document());
    REQUIRE(next.tasks().front().attempts().size() == 2);
    REQUIRE(next.tasks().front().state() == DeliveryTaskState::Delivered);
}
TEST_CASE(
    "DeliveryTask verifies representative while out for delivery and rolls back identity failures",
    "[delivery-task]") {
    ShipmentRegistry registry("BY");
    auto branch = test::deliveryBranch();
    auto courier = test::courier();
    const auto id = test::acceptCourierParcel(registry, branch);
    const auto rep = test::customer("rep");
    PickupAuthorization auth("a", registry.at(id), registry.at(id).recipient(), rep,
                             test::moment(20), test::moment(100));
    DeliveryRoute route("r", "courier");
    courier.assignTask(test::deliveryTask("t", registry, branch, id, courier), route);
    courier.startRoute(route, branch, registry, test::moment(20));
    const auto before = route;
    const auto history = registry.at(id).history();
    REQUIRE_THROWS_AS(route.completeDelivery(
                          "t", registry, courier, branch,
                          DeliveryAttempt("done", test::moment(30), DeliveryOutcome::Delivered),
                          test::deliveryPolicy(), rep, test::document()),
                      DeliveryAuthorizationException);
    REQUIRE(route == before);
    REQUIRE(registry.at(id).history() == history);
    route.completeDelivery("t", registry, courier, branch,
                           DeliveryAttempt("done", test::moment(30), DeliveryOutcome::Delivered),
                           test::deliveryPolicy(), rep, test::document(), &auth);
    REQUIRE(registry.at(id).status() == ShipmentStatus::Delivered);
}
