#include "post/shipments/DeliveryOptions.hpp"

#include "post/exceptions/IncompatibleServiceException.hpp"
#include "post/exceptions/PostalException.hpp"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <type_traits>
#include <vector>

using post::DeliveryOptions;
using post::Money;
using post::ServiceKind;
using post::ShipmentKind;

TEST_CASE("DeliveryOptions default to ordinary mail without paid extras", "[delivery_options]") {
    const DeliveryOptions options;
    REQUIRE_FALSE(options.registered());
    REQUIRE(options.declaredValue() == Money{});
    REQUIRE_FALSE(options.identityCheckRequired());
    REQUIRE_FALSE(options.courierDelivery());
    REQUIRE_FALSE(options.fragile());
    REQUIRE_FALSE(options.notificationRequired());
    REQUIRE(options.requiredServices().empty());
    REQUIRE_NOTHROW(options.validateCombination(ShipmentKind::Letter));
    REQUIRE_NOTHROW(options.validateCombination(ShipmentKind::Parcel));
    STATIC_REQUIRE(std::is_same_v<decltype(options.declaredValue()), const Money&>);
}

TEST_CASE("DeliveryOptions return each needed service once", "[delivery_options]") {
    const DeliveryOptions options(true, Money{10000}, true, true, true, true);
    REQUIRE(options.registered());
    REQUIRE(options.declaredValue() == Money{10000});
    REQUIRE(options.identityCheckRequired());
    REQUIRE(options.courierDelivery());
    REQUIRE(options.fragile());
    REQUIRE(options.notificationRequired());
    const std::vector expected{ServiceKind::Registration, ServiceKind::DeclaredValue,
                               ServiceKind::CourierDelivery, ServiceKind::FragileHandling,
                               ServiceKind::DeliveryNotification};
    REQUIRE(options.requiredServices() == expected);
    REQUIRE_NOTHROW(options.validateCombination(ShipmentKind::Parcel));
    REQUIRE_THROWS_AS(options.validateCombination(ShipmentKind::Letter),
                      post::IncompatibleServiceException);
    const DeliveryOptions registered(true, Money{}, true);
    REQUIRE(registered.requiredServices() == std::vector{ServiceKind::Registration});
}

TEST_CASE("DeliveryOptions check all flag combinations and shipment kinds", "[delivery_options]") {
    // Перебираем 64 набора: 5 логических условий и нулевую/ненулевую ценность.
    for (unsigned mask = 0; mask < 64; ++mask) {
        const bool registered = (mask & 1U) != 0;
        const bool value = (mask & 2U) != 0;
        const bool identity = (mask & 4U) != 0;
        const bool courier = (mask & 8U) != 0;
        const bool fragile = (mask & 16U) != 0;
        const bool notification = (mask & 32U) != 0;
        CAPTURE(mask);
        const bool commonValid =
            registered == identity && (registered || (!value && !notification));
        if (!commonValid) {
            REQUIRE_THROWS_AS(DeliveryOptions(registered, Money{value ? 1 : 0}, identity, courier,
                                              fragile, notification),
                              post::IncompatibleServiceException);
            continue;
        }
        const DeliveryOptions options(registered, Money{value ? 1 : 0}, identity, courier, fragile,
                                      notification);
        REQUIRE_NOTHROW(options.validateCombination(ShipmentKind::Parcel));
        if (courier || fragile) {
            REQUIRE_THROWS_AS(options.validateCombination(ShipmentKind::Letter),
                              post::IncompatibleServiceException);
        } else {
            REQUIRE_NOTHROW(options.validateCombination(ShipmentKind::Letter));
        }
        const auto services = options.requiredServices();
        const std::set unique(services.begin(), services.end());
        REQUIRE(unique.size() == services.size());
        REQUIRE(unique.contains(ServiceKind::Registration) == registered);
        REQUIRE(unique.contains(ServiceKind::DeclaredValue) == value);
        REQUIRE(unique.contains(ServiceKind::CourierDelivery) == courier);
        REQUIRE(unique.contains(ServiceKind::FragileHandling) == fragile);
        REQUIRE(unique.contains(ServiceKind::DeliveryNotification) == notification);
    }
}

TEST_CASE("DeliveryOptions reject unknown shipment kinds", "[delivery_options]") {
    const DeliveryOptions options;
    for (const int value : {-1, 2, 99}) {
        REQUIRE_THROWS_AS(options.validateCombination(static_cast<ShipmentKind>(value)),
                          post::IncompatibleServiceException);
    }
    REQUIRE_THROWS_AS(options.validateCombination(static_cast<ShipmentKind>(99)),
                      post::PostalException);
}

TEST_CASE("DeliveryOptions copies and failed operations preserve all flags", "[delivery_options]") {
    Money value{500};
    const DeliveryOptions original(true, value, true, true, false, true);
    value = Money{1000};
    REQUIRE(original.declaredValue() == Money{500});
    auto copy = original;
    REQUIRE(copy == original);
    REQUIRE_THROWS_AS(copy = DeliveryOptions(false, Money{500}),
                      post::IncompatibleServiceException);
    REQUIRE(copy == original);
    REQUIRE_THROWS_AS(copy.validateCombination(ShipmentKind::Letter),
                      post::IncompatibleServiceException);
    REQUIRE(copy == original);
    copy = DeliveryOptions{};
    REQUIRE(copy != original);
    REQUIRE(original.courierDelivery());
    REQUIRE(original.notificationRequired());
}
