#include "../LogisticsFixtures.hpp"
#include "post/detail/Time.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace post;
using namespace std::chrono;
TEST_CASE("RouteStop keeps explicit units and adds only handling time", "[route-stop]") {
    const RouteStop stop(" B ", 2, minutes{20}, minutes{5});
    REQUIRE(stop.branchId() == "B");
    REQUIRE(stop.sequence() == 2);
    REQUIRE(stop.travelMinutesFromPrevious() == minutes{20});
    REQUIRE(stop.handlingMinutes() == minutes{5});
    REQUIRE(stop.earliestDeparture(test::moment(100)) == test::moment(400));
    REQUIRE(RouteStop("A", 1, minutes{0}, minutes{0}).earliestDeparture(test::moment(0)) ==
            test::moment(0));
    REQUIRE(stop == stop);
    REQUIRE(stop != RouteStop("C", 2, minutes{20}, minutes{5}));
}
TEST_CASE("RouteStop validates first leg negative durations and representation boundaries",
          "[route-stop]") {
    REQUIRE_THROWS_AS(RouteStop("A", 0, minutes{0}, minutes{0}), std::invalid_argument);
    REQUIRE_THROWS_AS(RouteStop("A", 1, minutes{1}, minutes{0}), std::invalid_argument);
    REQUIRE_THROWS_AS(RouteStop("A", 2, minutes{-1}, minutes{0}), std::invalid_argument);
    REQUIRE_THROWS_AS(RouteStop("A", 2, minutes{0}, minutes{-1}), std::invalid_argument);
    REQUIRE_THROWS_AS(detail::secondsFromMinutes(minutes::max()), std::invalid_argument);
    REQUIRE_THROWS_AS(
        RouteStop("A", 1, minutes{0}, minutes{1}).earliestDeparture(sys_seconds{seconds::max()}),
        std::overflow_error);
    REQUIRE_THROWS_AS(RouteStop("A", 1, minutes{0}, minutes{0}).earliestDeparture(test::moment(-1)),
                      std::invalid_argument);
}
