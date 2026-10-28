#include "../LogisticsFixtures.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("StorageEntry accepts stored return phases even after informational events",
          "[storage-entry]") {
    for (const auto phase : {TrackingEventKind::ReturnStarted,
                             TrackingEventKind::PreparedForDispatch, TrackingEventKind::Arrived}) {
        auto registry = test::returningAt(phase);
        auto& shipment = registry.at("BY-1");
        test::advance(shipment, TrackingEventKind::Information, "C");
        REQUIRE_NOTHROW(StorageEntry(shipment, test::storagePolicy(), test::moment(20), "cell"));
    }
}
TEST_CASE("Storage rejects in-transit returns without changing cell or area", "[storage-entry]") {
    auto registry = test::returningAt(TrackingEventKind::Dispatched);
    auto& shipment = registry.at("BY-1");
    StorageCell cell("cell", 1000, 10, Dimensions(1000, 1000, 1000));
    auto area = test::storageArea();
    const auto cellBefore = cell;
    const auto areaBefore = area;
    for (const bool information : {false, true}) {
        if (information) {
            test::advance(shipment, TrackingEventKind::Information, "B");
        }
        REQUIRE_THROWS_AS(StorageEntry(shipment, test::storagePolicy(), test::moment(20), "cell"),
                          InvalidShipmentStateException);
        REQUIRE_THROWS_AS(cell.place(shipment, test::storagePolicy(), test::moment(20)),
                          InvalidShipmentStateException);
        REQUIRE_THROWS_AS(area.place(shipment, test::storagePolicy(), test::moment(20)),
                          InvalidShipmentStateException);
        REQUIRE(cell == cellBefore);
        REQUIRE(area == areaBefore);
    }
}
TEST_CASE("StorageEntry owns placement parameters and inclusive expiry boundary",
          "[storage-entry]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto& shipment = registry.at("BY-1");
    const auto policy = test::storagePolicy();
    const StorageEntry entry(shipment, policy, test::moment(10), " cell ");
    REQUIRE(entry.shipmentId() == "BY-1");
    REQUIRE(entry.storedWeightGrams() == 20);
    REQUIRE(entry.dimensions() == Dimensions(200, 100, 5));
    REQUIRE(entry.receivedAt() == test::moment(10));
    REQUIRE(entry.deadlineAt() == test::moment(10 + 3 * 86400));
    REQUIRE(entry.cellId() == "cell");
    REQUIRE_FALSE(entry.isOverdueAt(test::moment(10 + 3 * 86400 - 1)));
    REQUIRE(entry.isOverdueAt(test::moment(10 + 3 * 86400)));
    REQUIRE(entry.storageFeeUntil(policy, test::moment(10 + 86400)) == Money{});
    REQUIRE(entry.storageFeeUntil(policy, test::moment(10 + 86400 + 1)) == Money(50));
    REQUIRE(entry.storageFeeUntil(policy, test::moment(10 + 4 * 86400)) == Money(100));
    REQUIRE(entry == entry);
    REQUIRE(entry != StorageEntry(shipment, policy, test::moment(11), "other"));
}
TEST_CASE("StorageEntry rejects impossible placements and observations", "[storage-entry]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto policy = test::storagePolicy();
    const auto& shipment = registry.at("BY-1");
    const StorageEntry entry(shipment, policy, test::moment(10), "cell");
    REQUIRE_THROWS_AS(StorageEntry(test::letter(), policy, test::moment(10), "cell"),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(StorageEntry(shipment, policy, test::moment(0), "cell"),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(entry.isOverdueAt(test::moment(9)), std::invalid_argument);
    REQUIRE_THROWS_AS(entry.storageFeeUntil(policy, test::moment(9)), std::invalid_argument);
    auto transit = test::atStatus(ShipmentStatus::InTransit);
    REQUIRE_THROWS_AS(StorageEntry(transit.at("BY-1"), policy, test::moment(10), "cell"),
                      InvalidShipmentStateException);
    const StoragePolicy longer("longer", 0, 10, Money(1), 1, 0);
    REQUIRE(entry.storageFeeUntil(longer, test::moment(10 + 20 * 86400)) == Money(3));
}
