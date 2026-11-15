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
TEST_CASE("MailBag rechecks return cargo after another batch departs", "[mail-bag]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto letterId = test::registerCargo(registry);
    Parcel parcel(test::customer("sender", "220000"), test::customer("recipient", "230000"), 500,
                  DeliveryOptions{}, Dimensions(200, 100, 50), PackagingKind::Box);
    parcel.addItem(ContentItem("Книга", 1, 100, Money(200), ContentCategory::GeneralGoods));
    parcel.seal("parcel-seal");
    const auto parcelId =
        registry.registerShipment(parcel, 1000, Dimensions(1000, 1000, 1000), 100);
    for (const auto& id : {letterId, parcelId}) {
        test::accept(source, registry, id);
    }
    auto outbound = test::preparedBatch(registry, target, {letterId, parcelId});
    source.transferOut(registry, outbound, test::sortingPlan(), "worker", test::moment(30));
    target.receiveBatch(registry, outbound, "worker", test::storagePolicy(), test::moment(2200));
    for (const auto& id : {letterId, parcelId}) {
        ReturnRequest request("return-" + id);
        request.submit(registry.at(id), registry.at(id).recipient(), ReturnReason::RecipientRefused,
                       test::moment(2250));
        request.approve(registry.at(id), test::employee("worker", "B"), target,
                        test::storagePolicy(), test::moment(2260));
        request.beginReturn(target, registry, test::employee("worker", "B"), test::moment(2270));
    }
    MailBag pending("pending", 10000, "A");
    for (const auto& id : {letterId, parcelId}) {
        pending.add(registry.at(id), source, test::sortingPlan("B", "A"));
    }
    auto returning = test::preparedBatch(registry, source, {parcelId}, "B", 2300);
    target.transferOut(registry, returning, test::sortingPlan("B", "A"), "worker",
                       test::moment(2310));
    SECTION("Immediately after physical dispatch") {}
    SECTION("After a later informational event") {
        auto& shipment = registry.at(parcelId);
        shipment.applyTrackingEvent(TrackingEvent(
            shipment.history().events().back().sequence() + 1, TrackingEventKind::Information,
            test::moment(2320), "B", "Посылка находится в пути"));
    }
    REQUIRE(registry.at(parcelId).status() == ShipmentStatus::Returning);
    REQUIRE_THROWS_AS(target.storageEntry(parcelId), ShipmentNotFoundException);
    REQUIRE(target.storageEntry(letterId).shipmentId() == letterId);
    const auto before = pending;
    // Первый элемент ещё на складе; отказ на втором не должен частично пломбировать мешок.
    REQUIRE_THROWS_AS(pending.seal("stale-seal", test::moment(2330), registry),
                      InvalidShipmentStateException);
    REQUIRE(pending == before);
    REQUIRE_FALSE(pending.sealedAt());
    REQUIRE(pending.sealId().empty());
    pending.remove(parcelId);
    pending.seal("remaining-cargo", test::moment(2330), registry);
    REQUIRE(pending.sealedAt() == test::moment(2330));
    REQUIRE(pending.shipmentIds().size() == 1);
}
TEST_CASE("MailBag seals stored return phases without losing chronology checks", "[mail-bag]") {
    for (const auto phase : {TrackingEventKind::ReturnStarted,
                             TrackingEventKind::PreparedForDispatch, TrackingEventKind::Arrived}) {
        for (const bool information : {false, true}) {
            INFO("Return phase: " << static_cast<int>(phase) << "; information: " << information);
            auto registry = test::returningAt(
                phase == TrackingEventKind::Arrived ? TrackingEventKind::Dispatched : phase);
            if (phase == TrackingEventKind::Arrived) {
                test::advance(registry.at("BY-1"), TrackingEventKind::Arrived, "C");
            }
            const auto source = phase == TrackingEventKind::Arrived ? "C" : "B";
            MailBag bag("bag", 10000, "A");
            bag.add(registry.at("BY-1"), test::branch(), test::sortingPlan(source, "A"));
            const auto lastTime = registry.at("BY-1").history().events().back().occurredAt();
            if (information) {
                test::advance(registry.at("BY-1"), TrackingEventKind::Information, source);
            }
            // Информация не меняет фазу, но её время тоже ограничивает дату пломбы.
            const auto tooEarly = information ? lastTime : lastTime - std::chrono::seconds{1};
            const auto before = bag;
            REQUIRE_THROWS_AS(bag.seal("early", tooEarly, registry), InvalidShipmentStateException);
            REQUIRE(bag == before);
            bag.seal("seal", test::moment(20), registry);
            REQUIRE(bag.sealedAt() == test::moment(20));
            REQUIRE(bag.sealId() == "seal");
        }
    }
}
TEST_CASE("MailBag rejects changed cargo mass without modifying its snapshot", "[mail-bag]") {
    const auto registry = test::atStatus(ShipmentStatus::Accepted);
    MailBag bag("bag", 10000, "B");
    bag.add(registry.at("BY-1"), test::branch("B", "23"), test::sortingPlan());
    ShipmentRegistry replacement("BY");
    const auto id = test::registerCargo(replacement, 21);
    test::advance(replacement.at(id), TrackingEventKind::Accepted);
    const auto before = bag;
    REQUIRE_THROWS_AS(bag.seal("seal", test::moment(20), replacement),
                      InvalidShipmentStateException);
    REQUIRE(bag == before);
}
