#include "../BranchFixtures.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/PaymentException.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"
#include "post/exceptions/StorageCapacityException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("PostalBranch coordinates paid acceptance dispatch receipt and pickup",
          "[postal-branch]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerCargo(registry);
    auto source = test::branch();
    auto target = test::branch("B", "23");
    REQUIRE(source.id() == "A");
    REQUIRE(source.name() == "Отделение A");
    REQUIRE(source.address().postalCode() == "220000");
    REQUIRE(source.servedPostalPrefixes() == std::vector<std::string>{"22"});
    REQUIRE(source.employees().size() == 1);
    REQUIRE(source.workShifts().size() == 1);
    REQUIRE(source.storageAreas().size() == 1);
    REQUIRE(source.serves(registry.at(id).sender().address()));
    REQUIRE_FALSE(source.serves(registry.at(id).recipient().address()));
    test::accept(source, registry, id);
    REQUIRE(registry.at(id).status() == ShipmentStatus::Accepted);
    REQUIRE(source.storageEntry(id).receivedAt() == test::moment(15));
    auto batch = test::preparedBatch(registry, target);
    source.transferOut(registry, batch, test::sortingPlan(), "worker", test::moment(30));
    REQUIRE(registry.at(id).status() == ShipmentStatus::InTransit);
    REQUIRE_THROWS_AS(source.storageEntry(id), ShipmentNotFoundException);
    REQUIRE(batch.status() == BatchStatus::InTransit);
    target.receiveBatch(registry, batch, "worker", test::storagePolicy(), test::moment(2200));
    REQUIRE(registry.at(id).status() == ShipmentStatus::AwaitingPickup);
    REQUIRE(target.storageEntry(id).receivedAt() == test::moment(2200));
    const auto copiedTarget = target;
    target.issueShipment(registry, " BY-1 ", "worker", registry.at(id).recipient(),
                         test::document(), test::moment(2210));
    REQUIRE(registry.at(id).status() == ShipmentStatus::Delivered);
    REQUIRE_THROWS_AS(target.storageEntry(id), ShipmentNotFoundException);
    REQUIRE(copiedTarget.storageEntry(id).shipmentId() == id);
    REQUIRE(target != copiedTarget);
    REQUIRE(registry.at(id).history().timeSpentAt("A", test::moment(5000)) ==
            std::chrono::seconds{15});
    REQUIRE(registry.at(id).history().timeSpentAt("B", test::moment(5000)) ==
            std::chrono::seconds{10});
    REQUIRE_THROWS_AS(
        target.receiveBatch(registry, batch, "worker", test::storagePolicy(), test::moment(2220)),
        InvalidShipmentStateException);
}
TEST_CASE("PostalBranch acceptance rejects debt stale invoice time limits and full storage",
          "[postal-branch]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerCargo(registry);
    auto source = test::branch("A", "22", 1);
    const auto unpaid = test::invoice(registry.at(id));
    const auto paid = test::paidInvoice(registry, id);
    const auto before = source;
    REQUIRE_THROWS_AS(source.acceptShipment(registry, unpaid, id, "worker", test::storagePolicy(),
                                            1000, Dimensions(1000, 1000, 1000), 100,
                                            test::moment(15)),
                      PaymentException);
    for (const auto time : {9, 21}) {
        REQUIRE_THROWS_AS(source.acceptShipment(registry, paid, id, "worker", test::storagePolicy(),
                                                1000, Dimensions(1000, 1000, 1000), 100,
                                                test::moment(time)),
                          PaymentException);
    }
    REQUIRE_THROWS_AS(source.acceptShipment(registry, paid, id, "worker", test::storagePolicy(), 19,
                                            Dimensions(1000, 1000, 1000), 100, test::moment(15)),
                      PostalException);
    registry.at(id).revise(21, DeliveryOptions{});
    REQUIRE_THROWS_AS(source.acceptShipment(registry, paid, id, "worker", test::storagePolicy(),
                                            1000, Dimensions(1000, 1000, 1000), 100,
                                            test::moment(15)),
                      PaymentException);
    REQUIRE(source == before);
    REQUIRE(registry.at(id).status() == ShipmentStatus::Draft);
    test::accept(source, registry, id);
    const auto next = test::registerCargo(registry);
    const auto full = source;
    REQUIRE_THROWS_AS(test::accept(source, registry, next), StorageCapacityException);
    REQUIRE(source == full);
    REQUIRE(registry.at(next).status() == ShipmentStatus::Draft);
    REQUIRE(registry.at(next).history().events().empty());
}
TEST_CASE("PostalBranch validates configuration and source coverage", "[postal-branch]") {
    const auto address = test::customer().address();
    const auto make = [&](std::vector<StorageArea> areas, std::vector<Employee> staff,
                          std::vector<WorkShift> shifts,
                          std::vector<std::string> prefixes = {"22"}) {
        return PostalBranch("A", "Отделение", address, std::move(areas), std::move(staff),
                            std::move(shifts), std::move(prefixes));
    };
    REQUIRE_THROWS_AS(make({}, {}, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(make({test::storageArea(), test::storageArea()}, {}, {}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(make({test::storageArea()}, {test::employee(), test::employee()}, {}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(make({test::storageArea()}, {test::employee("w", "B")}, {}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(make({test::storageArea()}, {}, {}, {"23"}), std::invalid_argument);
    REQUIRE_THROWS_AS(make({test::storageArea()}, {}, {test::shift()}),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(
        make({test::storageArea()}, {test::employee()}, {test::shift(), test::shift()}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        make({test::storageArea()}, {test::employee()}, {test::shift(), test::shift("other")}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(make({test::storageArea()}, {test::employee()}, {test::shift("other", "B")}),
                      std::invalid_argument);
    auto area = test::storageArea();
    auto registered = test::atStatus(ShipmentStatus::Accepted);
    area.place(registered.at("BY-1"), test::storagePolicy(), test::moment(10));
    REQUIRE_THROWS_AS(make({area}, {}, {}), std::invalid_argument);
    ShipmentRegistry registry("BY");
    const auto id = test::registerCargo(registry);
    auto wrongSource = test::branch("B", "23");
    REQUIRE_THROWS_AS(test::accept(wrongSource, registry, id), InvalidShipmentStateException);
    REQUIRE(registry.at(id).status() == ShipmentStatus::Draft);
}
TEST_CASE("PostalBranch checks canonical staff and manages owned shifts safely",
          "[postal-branch]") {
    auto branch = test::branch();
    branch.addShiftBreak("shift", test::moment(10), test::moment(20));
    REQUIRE_THROWS_AS(branch.authorize("worker", BranchOperation::AcceptShipment, test::moment(15)),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(
        branch.authorize("unknown", BranchOperation::AcceptShipment, test::moment(25)),
        DeliveryAuthorizationException);
    REQUIRE_NOTHROW(branch.authorize("worker", BranchOperation::AcceptShipment, test::moment(20)));
    const auto before = branch;
    REQUIRE_THROWS_AS(branch.addShift(test::shift()), std::invalid_argument);
    REQUIRE(branch == before);
    branch.finishShift("shift", test::moment(30));
    REQUIRE_THROWS_AS(branch.authorize("worker", BranchOperation::AcceptShipment, test::moment(31)),
                      DeliveryAuthorizationException);
    branch.addShift(WorkShift("next", "worker", "A", test::moment(10000), test::moment(20000)));
    REQUIRE_THROWS_AS(branch.startShift("missing", test::moment(10000)), std::out_of_range);
    branch.startShift("next", test::moment(10000));
    REQUIRE_NOTHROW(
        branch.authorize("worker", BranchOperation::AcceptShipment, test::moment(10000)));
    auto courierBranch = test::branch("A", "22", 10, EmployeeRole::Courier);
    REQUIRE_THROWS_AS(
        courierBranch.authorize("worker", BranchOperation::AcceptShipment, test::moment(25)),
        DeliveryAuthorizationException);
}
TEST_CASE("PostalBranch rolls back every shipment and cell when later dispatch cargo fails",
          "[postal-branch]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto first = test::registerCargo(registry), second = test::registerCargo(registry);
    test::accept(source, registry, first);
    test::accept(source, registry, second);
    auto batch = test::preparedBatch(registry, target, {first, second});
    detail::appendFact(registry.at(second), TrackingEventKind::Information, test::moment(100), "A",
                       "Поздний факт");
    const auto branchBefore = source;
    const auto batchBefore = batch;
    const auto firstHistory = registry.at(first).history(),
               secondHistory = registry.at(second).history();
    REQUIRE_THROWS_AS(
        source.transferOut(registry, batch, test::sortingPlan(), "worker", test::moment(30)),
        InvalidShipmentStateException);
    REQUIRE(source == branchBefore);
    REQUIRE(batch == batchBefore);
    REQUIRE(registry.at(first).history() == firstHistory);
    REQUIRE(registry.at(second).history() == secondHistory);
    REQUIRE(registry.at(first).status() == ShipmentStatus::Accepted);
    REQUIRE(registry.at(second).status() == ShipmentStatus::Accepted);
    REQUIRE_THROWS_AS(
        target.transferOut(registry, batch, test::sortingPlan(), "worker", test::moment(110)),
        InvalidShipmentStateException);
    REQUIRE_THROWS_AS(
        source.transferOut(registry, batch, test::sortingPlan(), "worker", test::moment(19)),
        InvalidShipmentStateException);
    REQUIRE_THROWS_AS(source.transferOut(registry, batch, test::sortingPlan("A", "C"), "worker",
                                         test::moment(110)),
                      InvalidShipmentStateException);
}
TEST_CASE("PostalBranch receipt rolls back partial placement then succeeds on sufficient capacity",
          "[postal-branch]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23", 1);
    const auto first = test::registerCargo(registry), second = test::registerCargo(registry);
    test::accept(source, registry, first);
    test::accept(source, registry, second);
    auto batch = test::preparedBatch(registry, target, {first, second});
    source.transferOut(registry, batch, test::sortingPlan(), "worker", test::moment(30));
    const auto before = target;
    const auto batchBefore = batch;
    const auto firstHistory = registry.at(first).history(),
               secondHistory = registry.at(second).history();
    REQUIRE_THROWS_AS(
        target.receiveBatch(registry, batch, "worker", test::storagePolicy(), test::moment(2200)),
        StorageCapacityException);
    REQUIRE(target == before);
    REQUIRE(batch == batchBefore);
    REQUIRE(registry.at(first).history() == firstHistory);
    REQUIRE(registry.at(second).history() == secondHistory);
    REQUIRE(registry.at(first).status() == ShipmentStatus::InTransit);
    target = test::branch("B", "23", 2);
    target.receiveBatch(registry, batch, "worker", test::storagePolicy(), test::moment(2200));
    REQUIRE(target.storageAreas().front().cells().front().entries().size() == 2);
}
TEST_CASE("PostalBranch pickup rejects unauthorized or revoked representative without changes",
          "[postal-branch]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id);
    auto batch = test::preparedBatch(registry, target);
    source.transferOut(registry, batch, test::sortingPlan(), "worker", test::moment(30));
    target.receiveBatch(registry, batch, "worker", test::storagePolicy(), test::moment(2200));
    const auto rep = test::customer("rep");
    const auto before = target;
    const auto history = registry.at(id).history();
    REQUIRE_THROWS_AS(
        target.issueShipment(registry, id, "worker", rep, test::document(), test::moment(2205)),
        DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(target.issueShipment(registry, id, "worker", registry.at(id).recipient(),
                                           test::document("Другое имя"), test::moment(2205)),
                      DeliveryAuthorizationException);
    PickupAuthorization auth("auth", registry.at(id), registry.at(id).recipient(), rep,
                             test::moment(2200), test::moment(2300));
    auto revoked = auth;
    revoked.revoke(test::moment(2201));
    REQUIRE_THROWS_AS(target.issueShipment(registry, id, "worker", rep, test::document(),
                                           test::moment(2205), &revoked),
                      DeliveryAuthorizationException);
    REQUIRE(target == before);
    REQUIRE(registry.at(id).history() == history);
    target.issueShipment(registry, id, "worker", rep, test::document(), test::moment(2205), &auth);
    REQUIRE(registry.at(id).status() == ShipmentStatus::Delivered);
    REQUIRE_THROWS_AS(target.issueShipment(registry, id, "worker", rep, test::document(),
                                           test::moment(2206), &auth),
                      ShipmentNotFoundException);
}
TEST_CASE("PostalBranch stores intermediate arrivals without allowing pickup there",
          "[postal-branch]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto hub = test::branch("B", "24");
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id);
    auto batch = test::preparedBatch(registry, hub);
    source.transferOut(registry, batch, test::sortingPlan(), "worker", test::moment(30));
    hub.receiveBatch(registry, batch, "worker", test::storagePolicy(), test::moment(2200));
    REQUIRE(registry.at(id).status() == ShipmentStatus::Arrived);
    REQUIRE_THROWS_AS(hub.issueShipment(registry, id, "worker", registry.at(id).recipient(),
                                        test::document(), test::moment(2205)),
                      DeliveryAuthorizationException);
}
TEST_CASE("PostalBranch rejects registry disagreement and duplicate placement atomically",
          "[postal-branch]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id);
    const auto before = source;
    SECTION("A separate draft cannot occupy an already stored number") {
        ShipmentRegistry duplicate("BY");
        const auto duplicateId = test::registerCargo(duplicate);
        REQUIRE_THROWS_AS(test::accept(source, duplicate, duplicateId),
                          InvalidShipmentStateException);
        REQUIRE(duplicate.at(duplicateId).status() == ShipmentStatus::Draft);
        REQUIRE(duplicate.at(duplicateId).history().events().empty());
    }
    SECTION("Low-level events inconsistent with the warehouse are detected") {
        detail::appendFact(registry.at(id), TrackingEventKind::PreparedForDispatch,
                           test::moment(16), "A");
        detail::appendFact(registry.at(id), TrackingEventKind::Dispatched, test::moment(17), "A");
        detail::appendFact(registry.at(id), TrackingEventKind::Arrived, test::moment(18), "B");
        const auto history = registry.at(id).history();
        REQUIRE_THROWS_AS(source.issueShipment(registry, id, "worker", registry.at(id).recipient(),
                                               test::document(), test::moment(25)),
                          InvalidShipmentStateException);
        REQUIRE(registry.at(id).history() == history);
    }
    REQUIRE(source == before);
}
TEST_CASE("PostalBranch rejects a manifest from a different registry snapshot", "[postal-branch]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id);
    auto batch = test::preparedBatch(registry, target);
    source.transferOut(registry, batch, test::sortingPlan(), "worker", test::moment(30));
    ShipmentRegistry changed("BY");
    const auto changedId = test::registerCargo(changed, 30);
    detail::appendFact(changed.at(changedId), TrackingEventKind::Accepted, test::moment(15), "A");
    detail::appendFact(changed.at(changedId), TrackingEventKind::PreparedForDispatch,
                       test::moment(20), "A");
    detail::appendFact(changed.at(changedId), TrackingEventKind::Dispatched, test::moment(30), "A");
    const auto before = target;
    const auto batchBefore = batch;
    const auto history = changed.at(changedId).history();
    REQUIRE_THROWS_AS(
        target.receiveBatch(changed, batch, "worker", test::storagePolicy(), test::moment(2200)),
        InvalidShipmentStateException);
    REQUIRE(target == before);
    REQUIRE(batch == batchBefore);
    REQUIRE(changed.at(changedId).history() == history);
}
TEST_CASE("PostalBranch rolls back tracking exhaustion between preparation and departure",
          "[postal-branch]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id);
    registry.at(id).applyTrackingEvent(
        TrackingEvent(std::numeric_limits<std::int64_t>::max() - 1, TrackingEventKind::Information,
                      test::moment(19), "A", "Предел последовательности"));
    auto batch = test::preparedBatch(registry, target);
    const auto before = source;
    const auto batchBefore = batch;
    const auto history = registry.at(id).history();
    REQUIRE_THROWS_AS(
        source.transferOut(registry, batch, test::sortingPlan(), "worker", test::moment(30)),
        std::overflow_error);
    REQUIRE(source == before);
    REQUIRE(batch == batchBefore);
    REQUIRE(registry.at(id).history() == history);
    REQUIRE(registry.at(id).status() == ShipmentStatus::Accepted);
    REQUIRE_THROWS_AS(detail::lastOperationalFact(test::letter()), InvalidShipmentStateException);
}
TEST_CASE("PostalBranch tries compatible areas and issues local mail without a transportation loop",
          "[postal-branch]") {
    const StorageArea fragile("fragile", "Хрупкое", StorageAreaKind::Fragile,
                              {StorageCell("fragile-cell", 1000, 10, Dimensions(1000, 1000, 1000))},
                              {ShipmentKind::Parcel});
    PostalBranch branch("A", "Отделение", test::customer().address(),
                        {fragile, test::storageArea()}, {test::employee()}, {test::shift()},
                        {"22"});
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    test::accept(branch, registry, id);
    REQUIRE(branch.storageAreas().front().cells().front().entries().empty());
    REQUIRE(branch.storageAreas().back().cells().front().entries().size() == 1);
    REQUIRE(registry.at(id).status() == ShipmentStatus::AwaitingPickup);
    branch.issueShipment(registry, id, "worker", registry.at(id).recipient(), test::document(),
                         test::moment(25));
    REQUIRE(registry.at(id).status() == ShipmentStatus::Delivered);
}
TEST_CASE("PostalBranch accepts zero-price paid invoice and prepared dispatch", "[postal-branch]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto id = test::registerCargo(registry);
    const auto freeInvoice = test::invoice(registry.at(id), Money(0));
    source.acceptShipment(registry, freeInvoice, id, "worker", test::storagePolicy(), 1000,
                          Dimensions(1000, 1000, 1000), 100, test::moment(15));
    detail::appendFact(registry.at(id), TrackingEventKind::PreparedForDispatch, test::moment(16),
                       "A");
    auto batch = test::preparedBatch(registry, target);
    source.transferOut(registry, batch, test::sortingPlan(), "worker", test::moment(30));
    REQUIRE(registry.at(id).history().events().size() == 3);
}
