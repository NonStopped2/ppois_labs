#include "../LogisticsFixtures.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace post;
using namespace std::chrono;
TEST_CASE("PostalRoute checks directional reachability and an independent duration",
          "[postal-route]") {
    const auto route = test::route();
    REQUIRE(route.id() == "route");
    REQUIRE(route.name() == "Между отделениями");
    REQUIRE(route.stops().size() == 2);
    REQUIRE(route.maxDuration() == minutes{30});
    REQUIRE(route.allowedVehicleTypes() == std::vector{VehicleType::Van, VehicleType::Truck});
    REQUIRE(route.estimateDuration() == minutes{5 + 20 + 5});
    REQUIRE_NOTHROW(route.validateSequence());
    REQUIRE(route.connects(" A ", "B"));
    REQUIRE_FALSE(route.connects("B", "A"));
    REQUIRE_FALSE(route.connects("A", "A"));
    REQUIRE_FALSE(route.connects("missing", "B"));
    REQUIRE_FALSE(route.connects("A", "missing"));
    auto copy = route;
    REQUIRE(copy == route);
    copy = PostalRoute("new", "Новый", route.stops(), minutes{40}, {VehicleType::Van});
    REQUIRE(copy != route);
}
TEST_CASE("PostalRoute rejects invalid coverage ordering and configuration", "[postal-route]") {
    const auto stops = test::route().stops();
    REQUIRE_THROWS_AS(PostalRoute("r", " ", stops, minutes{30}, {VehicleType::Van}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(PostalRoute("r", "r", {}, minutes{30}, {VehicleType::Van}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(PostalRoute("r", "r", stops, minutes{29}, {VehicleType::Van}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(PostalRoute("r", "r", stops, minutes{-1}, {VehicleType::Van}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(PostalRoute("r", "r", stops, minutes{30}, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(PostalRoute("r", "r", stops, minutes{30}, {static_cast<VehicleType>(99)}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        PostalRoute("r", "r", stops, minutes{30}, {VehicleType::Van, VehicleType::Van}),
        std::invalid_argument);
    REQUIRE_THROWS_AS(PostalRoute("r", "r", {stops[1], stops[0]}, minutes{30}, {VehicleType::Van}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(PostalRoute("r", "r", {stops[0], RouteStop("A", 2, minutes{1}, minutes{0})},
                                  minutes{30}, {VehicleType::Van}),
                      std::invalid_argument);
}
TEST_CASE("PostalRoute detects extreme aggregate duration overflow", "[postal-route]") {
    const auto maxMinutes = std::numeric_limits<seconds::rep>::max() / 60;
    std::vector<RouteStop> stops;
    for (int sequence = 1; sequence <= 70; ++sequence) {
        stops.emplace_back("branch-" + std::to_string(sequence), sequence, minutes{0},
                           minutes{maxMinutes});
    }
    REQUIRE_THROWS_AS(PostalRoute("r", "r", stops, minutes{maxMinutes}, {VehicleType::Truck}),
                      std::overflow_error);
}
