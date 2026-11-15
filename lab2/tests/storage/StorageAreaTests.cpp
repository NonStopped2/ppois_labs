#include "../LogisticsFixtures.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"
#include "post/exceptions/StorageCapacityException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("StorageArea plans across cells without changing actual occupancy", "[storage-area]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto other = test::registerLetter(registry);
    test::advance(registry.at(other), TrackingEventKind::Accepted);
    StorageArea area(" a ", " Общая  зона ", StorageAreaKind::General,
                     {StorageCell("c1", 20, 1, Dimensions(200, 100, 5)),
                      StorageCell("c2", 20, 1, Dimensions(200, 100, 5))},
                     {ShipmentKind::Letter});
    REQUIRE(area.id() == "a");
    REQUIRE(area.name() == "Общая зона");
    REQUIRE(area.kind() == StorageAreaKind::General);
    REQUIRE(area.allowedShipmentKinds() == std::vector{ShipmentKind::Letter});
    const auto before = area;
    const auto plan =
        area.planPlacement({std::cref(registry.at("BY-1")), std::cref(registry.at(other))},
                           test::storagePolicy(), test::moment(10));
    REQUIRE(plan.at("BY-1") == "c1");
    REQUIRE(plan.at(other) == "c2");
    REQUIRE(area == before);
    REQUIRE_THROWS_AS(
        area.planPlacement({std::cref(registry.at(other)), std::cref(registry.at(other))},
                           test::storagePolicy(), test::moment(10)),
        StorageCapacityException);
    REQUIRE(area == before);
    area.place(registry.at("BY-1"), test::storagePolicy(), test::moment(10));
    REQUIRE(area.cells()[0].entries().size() == 1);
    REQUIRE(area.occupancyByKind(registry).at(ShipmentKind::Letter) == 1);
    REQUIRE_THROWS_AS(area.place(registry.at("BY-1"), test::storagePolicy(), test::moment(10)),
                      StorageCapacityException);
    REQUIRE_THROWS_AS(area.planPlacement({std::cref(registry.at("BY-1"))}, test::storagePolicy(),
                                         test::moment(10)),
                      StorageCapacityException);
    auto copy = area;
    REQUIRE(copy.take("BY-1").cellId() == "c1");
    REQUIRE(area.contains("BY-1"));
    REQUIRE(copy != area);
    REQUIRE_THROWS_AS(copy.take("unknown"), ShipmentNotFoundException);
}
TEST_CASE("StorageArea rejects overload and incompatible products atomically", "[storage-area]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    auto area = test::storageArea("area", 1);
    const auto other = test::registerLetter(registry);
    test::advance(registry.at(other), TrackingEventKind::Accepted);
    const auto before = area;
    REQUIRE_THROWS_AS(
        area.planPlacement({std::cref(registry.at("BY-1")), std::cref(registry.at(other))},
                           test::storagePolicy(), test::moment(10)),
        StorageCapacityException);
    REQUIRE(area == before);
    const StorageArea parcelOnly("p", "p", StorageAreaKind::General,
                                 {StorageCell("c", 1000, 10, Dimensions(1000, 1000, 1000))},
                                 {ShipmentKind::Parcel});
    REQUIRE_THROWS_AS(parcelOnly.planPlacement({std::cref(registry.at("BY-1"))},
                                               test::storagePolicy(), test::moment(10)),
                      StorageCapacityException);
    const StorageArea fragileOnly("f", "f", StorageAreaKind::Fragile,
                                  {StorageCell("c", 1000, 10, Dimensions(1000, 1000, 1000))},
                                  {ShipmentKind::Parcel});
    const auto parcelId =
        registry.registerShipment(test::parcel(), 1000, Dimensions(1000, 1000, 1000), 10);
    test::advance(registry.at(parcelId), TrackingEventKind::Accepted);
    REQUIRE_THROWS_AS(fragileOnly.planPlacement({std::cref(registry.at(parcelId))},
                                                test::storagePolicy(), test::moment(10)),
                      StorageCapacityException);
}
TEST_CASE("StorageArea validates configuration including duplicates and nonempty cells",
          "[storage-area]") {
    const auto cell = StorageCell("c", 100, 1, Dimensions(200, 100, 5));
    REQUIRE_THROWS_AS(
        StorageArea("a", " ", StorageAreaKind::General, {cell}, {ShipmentKind::Letter}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        StorageArea("a", "a", static_cast<StorageAreaKind>(99), {cell}, {ShipmentKind::Letter}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(StorageArea("a", "a", StorageAreaKind::General, {}, {ShipmentKind::Letter}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        StorageArea("a", "a", StorageAreaKind::General, {cell, cell}, {ShipmentKind::Letter}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(StorageArea("a", "a", StorageAreaKind::General, {cell}, {}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        StorageArea("a", "a", StorageAreaKind::General, {cell}, {static_cast<ShipmentKind>(99)}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(StorageArea("a", "a", StorageAreaKind::General, {cell},
                                  {ShipmentKind::Letter, ShipmentKind::Letter}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        StorageArea("a", "a", StorageAreaKind::Fragile, {cell}, {ShipmentKind::Letter}),
        std::invalid_argument);
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    auto occupied = cell;
    occupied.place(registry.at("BY-1"), test::storagePolicy(), test::moment(10));
    REQUIRE_THROWS_AS(
        StorageArea("a", "a", StorageAreaKind::General, {occupied}, {ShipmentKind::Letter}),
        std::invalid_argument);
}
