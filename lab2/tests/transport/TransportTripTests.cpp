#include "../TransportFixtures.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include "post/exceptions/StorageCapacityException.hpp"
#include "post/exceptions/TransportUnavailableException.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace post;
TEST_CASE("TransportTrip enforces handling travel stop order and complete unloading",
          "[transport-trip]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id);
    auto car = test::vehicle();
    TransportTrip trip(" trip ", test::route());
    REQUIRE(trip.id() == "trip");
    REQUIRE(trip.route() == test::route());
    REQUIRE(trip.vehicleId().empty());
    REQUIRE(trip.state() == TripState::Planned);
    REQUIRE_FALSE(trip.actualDeparture());
    REQUIRE_FALSE(trip.actualArrival());
    REQUIRE_FALSE(trip.lastActivityAt());
    REQUIRE(trip.currentStopIndex() == 0);
    REQUIRE(trip.totalWeightGrams() == 0);
    REQUIRE(trip.totalBags() == 0);
    trip.loadBatch(test::batchFor(registry, target, {id}), car);
    REQUIRE(trip.vehicleId() == car.id());
    REQUIRE(trip.totalWeightGrams() == 20);
    REQUIRE(trip.totalBags() == 1);
    const auto manifest = TransportManifest::buildFor("m", trip, test::moment(20));
    const auto inspection = test::inspection(car);
    const auto before = trip;
    REQUIRE_THROWS_AS(trip.depart(car, inspection, manifest, source, registry, test::sortingPlan(),
                                  "worker", test::moment(319)),
                      InvalidShipmentStateException);
    REQUIRE(trip == before);
    REQUIRE(car.state() == VehicleState::Available);
    REQUIRE(source.storageEntry(id).shipmentId() == id);
    trip.depart(car, inspection, manifest, source, registry, test::sortingPlan(), "worker",
                test::moment(320));
    REQUIRE(trip.actualDeparture() == test::moment(320));
    REQUIRE_THROWS_AS(trip.depart(car, inspection, manifest, source, registry, test::sortingPlan(),
                                  "worker", test::moment(321)),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(trip.arriveAt(source, test::moment(1520)), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(trip.arriveAt(target, test::moment(1519)), InvalidShipmentStateException);
    trip.arriveAt(target, test::moment(1520));
    REQUIRE(trip.state() == TripState::Arrived);
    REQUIRE(trip.currentStopIndex() == 1);
    REQUIRE_THROWS_AS(
        trip.unloadFor(target, registry, "worker", test::storagePolicy(), test::moment(1819)),
        InvalidShipmentStateException);
    REQUIRE(trip.unloadFor(target, registry, "worker", test::storagePolicy(), test::moment(1820)) ==
            1);
    REQUIRE(trip.state() == TripState::Completed);
    REQUIRE(trip.actualArrival() == test::moment(1520));
    REQUIRE(trip.lastActivityAt() == test::moment(1820));
    REQUIRE(trip.batches().front().status() == BatchStatus::Received);
    REQUIRE(registry.at(id).status() == ShipmentStatus::AwaitingPickup);
    REQUIRE_THROWS_AS(trip.arriveAt(target, test::moment(1821)), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(
        trip.unloadFor(target, registry, "worker", test::storagePolicy(), test::moment(1821)),
        InvalidShipmentStateException);
    REQUIRE_NOTHROW(manifest.verifyAgainst(trip));
}
TEST_CASE(
    "TransportTrip rejects duplicate cargo wrong machine type capacity and loading after departure",
    "[transport-trip]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto target = test::branch("B", "23");
    const auto batch = test::batchFor(registry, target, {"BY-1"});
    auto car = test::vehicle();
    TransportTrip trip("t", test::route());
    REQUIRE_THROWS_AS(trip.loadBatch(batch, test::vehicle("small", 19)),
                      TransportUnavailableException);
    trip.loadBatch(batch, car);
    const auto before = trip;
    REQUIRE_THROWS_AS(trip.loadBatch(batch, car), InvalidShipmentStateException);
    REQUIRE(trip == before);
    REQUIRE_THROWS_AS(trip.loadBatch(test::batchFor(registry, target, {"BY-1"}, "other"), car),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(TransportTrip("other", test::routeBetween("B", "A")).loadBatch(batch, car),
                      InvalidShipmentStateException);
    const PostalRoute trucks("r", "Только грузовики", test::route().stops(),
                             std::chrono::minutes{30}, {VehicleType::Truck});
    REQUIRE_THROWS_AS(TransportTrip("truck", trucks).loadBatch(batch, car),
                      TransportUnavailableException);
}
TEST_CASE("TransportTrip departure rolls back all collaborators when later cargo is invalid",
          "[transport-trip]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    auto car = test::vehicle();
    const auto first = test::registerCargo(registry), second = test::registerCargo(registry);
    test::accept(source, registry, first);
    test::accept(source, registry, second);
    TransportTrip trip("t", test::route());
    trip.loadBatch(test::batchFor(registry, target, {first}, "one"), car);
    trip.loadBatch(test::batchFor(registry, target, {second}, "two"), car);
    const auto manifest = TransportManifest::buildFor("m", trip, test::moment(20));
    detail::appendFact(registry.at(second), TrackingEventKind::Information, test::moment(500), "A",
                       "Будущий факт");
    const auto before = trip;
    const auto branchBefore = source;
    const auto carBefore = car;
    const auto history = registry.at(first).history();
    REQUIRE_THROWS_AS(trip.depart(car, test::inspection(car), manifest, source, registry,
                                  test::sortingPlan(), "worker", test::moment(320)),
                      InvalidShipmentStateException);
    REQUIRE(trip == before);
    REQUIRE(source == branchBefore);
    REQUIRE(car == carBefore);
    REQUIRE(registry.at(first).history() == history);
}
TEST_CASE("TransportTrip visits intermediate stops and accounts for delayed unloading",
          "[transport-trip]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto b = test::branch("B", "23");
    auto c = test::branch("C", "24");
    const auto first = test::registerCargo(registry);
    const Letter prototype(test::customer("sender"), test::customer("recipient", "240000"), 20,
                           DeliveryOptions{}, Dimensions(200, 100, 5), 2, "Документы");
    const auto second =
        registry.registerShipment(prototype, 1000, Dimensions(1000, 1000, 1000), 100);
    test::accept(source, registry, first);
    test::accept(source, registry, second);
    const PostalRoute route("r", "Три пункта",
                            {RouteStop("A", 1, std::chrono::minutes{0}, std::chrono::minutes{1}),
                             RouteStop("B", 2, std::chrono::minutes{2}, std::chrono::minutes{1}),
                             RouteStop("C", 3, std::chrono::minutes{3}, std::chrono::minutes{1})},
                            std::chrono::minutes{8}, {VehicleType::Van});
    const SortingPlan plan("p", "A",
                           {SortingRule("b", {"23"}, "B", 1, {ShipmentKind::Letter}),
                            SortingRule("c", {"24"}, "C", 1, {ShipmentKind::Letter})},
                           1);
    auto car = test::vehicle("car", 40, 2);
    TransportTrip trip("t", route);
    trip.loadBatch(test::batchFor(registry, b, {first}, "b"), car);
    trip.loadBatch(test::batchFor(registry, c, {second}, "c"), car);
    const auto manifest = TransportManifest::buildFor("m", trip, test::moment(20));
    trip.depart(car, test::inspection(car), manifest, source, registry, plan, "worker",
                test::moment(80));
    REQUIRE_THROWS_AS(trip.loadBatch(trip.batches().front(), car), InvalidShipmentStateException);
    trip.arriveAt(b, test::moment(200));
    REQUIRE_THROWS_AS(trip.arriveAt(c, test::moment(440)), InvalidShipmentStateException);
    REQUIRE(trip.unloadFor(b, registry, "worker", test::storagePolicy(), test::moment(400)) == 1);
    REQUIRE_THROWS_AS(
        trip.unloadFor(b, registry, "worker", test::storagePolicy(), test::moment(401)),
        InvalidShipmentStateException);
    REQUIRE_THROWS_AS(trip.arriveAt(c, test::moment(579)), InvalidShipmentStateException);
    trip.arriveAt(c, test::moment(580));
    REQUIRE(trip.unloadFor(c, registry, "worker", test::storagePolicy(), test::moment(640)) == 1);
    REQUIRE(trip.state() == TripState::Completed);
    REQUIRE(trip.lastActivityAt() == test::moment(640));
}
TEST_CASE("TransportTrip limits accumulated bags and mass and does not switch reserved vehicles",
          "[transport-trip]") {
    ShipmentRegistry registry("BY");
    const auto first = test::registerCargo(registry), second = test::registerCargo(registry);
    test::advance(registry.at(first), TrackingEventKind::Accepted);
    test::advance(registry.at(second), TrackingEventKind::Accepted);
    const auto target = test::branch("B", "23");
    auto car = test::vehicle("car", 40, 1);
    TransportTrip trip("t", test::route());
    trip.loadBatch(test::batchFor(registry, target, {first}, "first"), car);
    const auto before = trip;
    REQUIRE_THROWS_AS(trip.loadBatch(test::batchFor(registry, target, {second}, "second"), car),
                      TransportUnavailableException);
    REQUIRE(trip == before);
    REQUIRE_THROWS_AS(trip.loadBatch(test::batchFor(registry, target, {second}, "second"),
                                     test::vehicle("other")),
                      TransportUnavailableException);
    car.assignTo(trip, test::inspection(car), test::moment(20));
    REQUIRE_THROWS_AS(TransportTrip("other", test::route()).loadBatch(trip.batches().front(), car),
                      TransportUnavailableException);
}
TEST_CASE(
    "TransportTrip unload uses total capacity across all batches and preserves original snapshots",
    "[transport-trip]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23", 1);
    auto car = test::vehicle();
    const auto first = test::registerCargo(registry), second = test::registerCargo(registry);
    test::accept(source, registry, first);
    test::accept(source, registry, second);
    TransportTrip trip("t", test::route());
    trip.loadBatch(test::batchFor(registry, target, {first}, "one"), car);
    trip.loadBatch(test::batchFor(registry, target, {second}, "two"), car);
    const auto manifest = TransportManifest::buildFor("m", trip, test::moment(20));
    trip.depart(car, test::inspection(car), manifest, source, registry, test::sortingPlan(),
                "worker", test::moment(320));
    trip.arriveAt(target, test::moment(1520));
    const auto before = trip;
    const auto branchBefore = target;
    const auto history = registry.at(first).history();
    REQUIRE_THROWS_AS(
        trip.unloadFor(target, registry, "worker", test::storagePolicy(), test::moment(1820)),
        StorageCapacityException);
    REQUIRE(trip == before);
    REQUIRE(target == branchBefore);
    REQUIRE(registry.at(first).history() == history);
    auto larger = test::branch("B", "23", 2);
    auto copy = trip;
    REQUIRE(copy.unloadFor(larger, registry, "worker", test::storagePolicy(), test::moment(1820)) ==
            2);
    REQUIRE(trip.state() == TripState::Arrived);
    REQUIRE(copy.state() == TripState::Completed);
}
