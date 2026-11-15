#include "../TransportFixtures.hpp"
#include "post/exceptions/TransportUnavailableException.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace post;
TEST_CASE("Vehicle owns positive capacity registration and assignment", "[vehicle]") {
    auto car = test::vehicle();
    REQUIRE(car.id() == "car");
    REQUIRE(car.registrationNumber() == "AB-123");
    REQUIRE(car.maxWeightGrams() == 10000);
    REQUIRE(car.maxBags() == 10);
    REQUIRE(car.type() == VehicleType::Van);
    REQUIRE(car.state() == VehicleState::Available);
    REQUIRE_FALSE(car.assignedTripId());
    REQUIRE(car == car);
    REQUIRE_THROWS_AS(Vehicle("v", "n", 0, 1, VehicleType::Van), std::invalid_argument);
    REQUIRE_THROWS_AS(Vehicle("v", "n", 1, 0, VehicleType::Van), std::invalid_argument);
    REQUIRE_THROWS_AS(Vehicle("v", "n", 1, 1, static_cast<VehicleType>(99)), std::invalid_argument);
}
TEST_CASE("Vehicle reservation cannot be stolen or released before unloading", "[vehicle]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id);
    auto car = test::vehicle();
    const auto inspection = test::inspection(car);
    TransportTrip trip("trip", test::route());
    trip.loadBatch(test::batchFor(registry, target, {id}), car);
    car.assignTo(trip, inspection, test::moment(20));
    REQUIRE(car.state() == VehicleState::Assigned);
    REQUIRE(car.assignedTripId() == "trip");
    const auto reserved = car;
    REQUIRE_THROWS_AS(car.assignTo(trip, inspection, test::moment(20)),
                      TransportUnavailableException);
    REQUIRE_THROWS_AS(car.releaseFrom(trip), TransportUnavailableException);
    REQUIRE(car == reserved);
    const auto manifest = TransportManifest::buildFor("m", trip, test::moment(20));
    trip.depart(car, inspection, manifest, source, registry, test::sortingPlan(), "worker",
                test::moment(320));
    trip.arriveAt(target, test::moment(1520));
    REQUIRE(trip.unloadFor(target, registry, "worker", test::storagePolicy(), test::moment(1820)) ==
            1);
    car.releaseFrom(trip);
    REQUIRE(car.state() == VehicleState::Available);
    REQUIRE_FALSE(car.assignedTripId());
    REQUIRE_THROWS_AS(car.releaseFrom(trip), TransportUnavailableException);
}
