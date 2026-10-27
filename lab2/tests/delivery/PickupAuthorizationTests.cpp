#include "../StaffFixtures.hpp"
#include "post/detail/Time.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace post;
TEST_CASE("PickupAuthorization binds shipment principal representative and inclusive time",
          "[pickup-authorization]") {
    auto registry = test::atStatus(ShipmentStatus::AwaitingPickup);
    const auto& shipment = registry.at("BY-1");
    const auto principal = shipment.recipient();
    const auto representative = test::customer("representative");
    PickupAuthorization auth(" auth ", shipment, principal, representative, test::moment(10),
                             test::moment(20));
    REQUIRE(auth.id() == "auth");
    REQUIRE(auth.shipmentId() == "BY-1");
    REQUIRE(auth.principalCustomerId() == "recipient");
    REQUIRE(auth.representativeCustomerId() == "representative");
    REQUIRE(auth.validFrom() == test::moment(10));
    REQUIRE(auth.validUntil() == test::moment(20));
    REQUIRE_FALSE(auth.revokedAt());
    REQUIRE_NOTHROW(
        auth.validateFor(shipment, principal, representative, test::document(), test::moment(10)));
    REQUIRE_NOTHROW(
        auth.validateFor(shipment, principal, representative, test::document(), test::moment(20)));
    const auto before = auth;
    REQUIRE_THROWS_AS(
        auth.validateFor(shipment, principal, representative, test::document(), test::moment(9)),
        DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(
        auth.validateFor(shipment, principal, representative, test::document(), test::moment(21)),
        DeliveryAuthorizationException);
    auth.revoke(test::moment(15));
    REQUIRE(auth.revokedAt() == test::moment(15));
    REQUIRE(auth != before);
    REQUIRE_THROWS_AS(
        auth.validateFor(shipment, principal, representative, test::document(), test::moment(10)),
        DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(auth.revoke(test::moment(16)), DeliveryAuthorizationException);
}
TEST_CASE("PickupAuthorization rejects wrong people documents scope and state",
          "[pickup-authorization]") {
    auto registry = test::atStatus(ShipmentStatus::AwaitingPickup);
    const auto& shipment = registry.at("BY-1");
    const auto rep = test::customer("rep");
    const PickupAuthorization auth("a", shipment, shipment.recipient(), rep, test::moment(10),
                                   test::moment(20));
    REQUIRE_THROWS_AS(auth.validateFor(shipment, test::customer("wrong"), rep, test::document(),
                                       test::moment(10)),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(auth.validateFor(shipment, shipment.recipient(), test::customer("wrong"),
                                       test::document(), test::moment(10)),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(auth.validateFor(shipment, shipment.recipient(), rep,
                                       test::document("Другой человек"), test::moment(10)),
                      DeliveryAuthorizationException);
    auto other = test::atStatus(ShipmentStatus::Delivered);
    REQUIRE_THROWS_AS(auth.validateFor(other.at("BY-1"), shipment.recipient(), rep,
                                       test::document(), test::moment(10)),
                      DeliveryAuthorizationException);
    ShipmentRegistry otherRegistry("OTHER");
    const auto id = test::registerLetter(otherRegistry);
    REQUIRE_THROWS_AS(auth.validateFor(otherRegistry.at(id), shipment.recipient(), rep,
                                       test::document(), test::moment(10)),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(
        PickupAuthorization("a", shipment, rep, rep, test::moment(10), test::moment(20)),
        DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(PickupAuthorization("a", shipment, shipment.recipient(), rep,
                                          test::moment(20), test::moment(10)),
                      DeliveryAuthorizationException);
}
TEST_CASE("PickupAuthorization uses sender for return and protects calendar conversion",
          "[pickup-authorization]") {
    auto registry = test::atStatus(ShipmentStatus::Returning);
    const auto& shipment = registry.at("BY-1");
    const auto rep = test::customer("rep");
    const PickupAuthorization auth("a", shipment, shipment.sender(), rep, test::moment(0),
                                   test::moment(std::numeric_limits<std::int64_t>::max()));
    REQUIRE_NOTHROW(
        auth.validateFor(shipment, shipment.sender(), rep, test::document(), test::moment(20)));
    REQUIRE_THROWS_AS(auth.validateFor(shipment, shipment.sender(), rep, test::document(),
                                       test::moment(std::numeric_limits<std::int64_t>::max())),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(detail::dateAt(test::moment(-1)), std::invalid_argument);
    REQUIRE(detail::dateAt(test::moment(0)) == std::chrono::year{1970} / std::chrono::January / 1);
}
