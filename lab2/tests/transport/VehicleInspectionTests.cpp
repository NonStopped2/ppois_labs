#include "../TransportFixtures.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/TransportUnavailableException.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace post;
TEST_CASE("VehicleInspection tracks defects and revokes approval until resolved",
          "[vehicle-inspection]") {
    const auto car = test::vehicle();
    VehicleInspection inspection(" i ", car, test::moment(10), test::moment(20));
    REQUIRE(inspection.id() == "i");
    REQUIRE(inspection.vehicleId() == car.id());
    REQUIRE(inspection.inspectedAt() == test::moment(10));
    REQUIRE(inspection.expiresAt() == test::moment(20));
    REQUIRE_FALSE(inspection.approved());
    REQUIRE(inspection.inspectorId().empty());
    inspection.addDefect(" Тормоза ");
    REQUIRE_FALSE(inspection.defects().at("Тормоза"));
    const auto before = inspection;
    REQUIRE_THROWS_AS(inspection.approve(test::employee(), test::moment(10)),
                      TransportUnavailableException);
    REQUIRE(inspection == before);
    REQUIRE_THROWS_AS(inspection.addDefect("Тормоза"), TransportUnavailableException);
    REQUIRE_THROWS_AS(inspection.addDefect(" "), std::invalid_argument);
    REQUIRE_THROWS_AS(inspection.resolveDefect("Неизвестно"), TransportUnavailableException);
    inspection.resolveDefect("Тормоза");
    REQUIRE_THROWS_AS(inspection.resolveDefect("Тормоза"), TransportUnavailableException);
    inspection.approve(test::employee("inspector", "A", EmployeeRole::Inspector), test::moment(10));
    REQUIRE(inspection.approved());
    REQUIRE(inspection.inspectorId() == "inspector");
    REQUIRE_NOTHROW(inspection.validateFor(car, test::moment(10)));
    REQUIRE_NOTHROW(inspection.validateFor(car, test::moment(20)));
    REQUIRE_THROWS_AS(inspection.validateFor(car, test::moment(9)), TransportUnavailableException);
    REQUIRE_THROWS_AS(inspection.validateFor(car, test::moment(21)), TransportUnavailableException);
    REQUIRE_THROWS_AS(inspection.validateFor(test::vehicle("other"), test::moment(15)),
                      TransportUnavailableException);
    auto copy = inspection;
    copy.addDefect("Шины");
    REQUIRE(copy != inspection);
    REQUIRE(inspection.approved());
}
TEST_CASE("VehicleInspection rejects invalid interval authority and repeated approval",
          "[vehicle-inspection]") {
    const auto car = test::vehicle();
    REQUIRE_THROWS_AS(VehicleInspection("i", car, test::moment(20), test::moment(10)),
                      std::invalid_argument);
    VehicleInspection inspection("i", car, test::moment(10), test::moment(20));
    REQUIRE_THROWS_AS(
        inspection.approve(test::employee("clerk", "A", EmployeeRole::Clerk), test::moment(10)),
        DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(inspection.approve(test::employee(), test::moment(9)),
                      TransportUnavailableException);
    inspection.approve(test::employee(), test::moment(20));
    REQUIRE_THROWS_AS(inspection.approve(test::employee(), test::moment(20)),
                      TransportUnavailableException);
}
