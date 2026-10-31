#include "post/detail/PricingMath.hpp"
#include "post/exceptions/IncompatibleServiceException.hpp"
#include "post/tariffs/ServiceRate.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <vector>

using namespace post;
TEST_CASE("ServiceRate keeps mode data and caps after rounding", "[service-rate]") {
    const ServiceRate fixed(ServiceKind::Registration, RateMode::Fixed, Money(101), 0, Money(105));
    REQUIRE(fixed.service() == ServiceKind::Registration);
    REQUIRE(fixed.mode() == RateMode::Fixed);
    REQUIRE(fixed.fixedCharge() == Money(101));
    REQUIRE(fixed.basisPoints() == 0);
    REQUIRE(fixed.maximumCharge() == std::optional<Money>(Money(105)));
    REQUIRE(fixed.calculateSurcharge(Money(999), 10) == Money(105));
    const ServiceRate percentage(ServiceKind::DeclaredValue, RateMode::Percentage, Money{}, 100);
    REQUIRE(percentage.calculateSurcharge(Money(12345)) == Money(124));
    REQUIRE(percentage.calculateSurcharge(Money(12345), 10) == Money(130));
    REQUIRE(percentage != fixed);
    REQUIRE(percentage == percentage);
}
TEST_CASE("ServiceRate matches an independent small-integer rounding formula", "[service-rate]") {
    const std::vector<std::optional<Money>> caps{std::nullopt, Money{}, Money(1), Money(125),
                                                 Money(20000)};
    for (const std::int64_t basis : {0, 1, 99, 100, 101, 9999, 10000, 10001, 12345}) {
        for (const std::int64_t factor : {0, 1, 99, 100, 3333, 10000, 10001, 20000}) {
            for (const std::int64_t step : {1, 5, 10}) {
                for (const auto cap : caps) {
                    const auto kopecks = (basis * factor + 9999) / 10000;
                    auto expected = (kopecks + step - 1) / step * step;
                    if (cap) {
                        expected = std::min(expected, cap->kopecks());
                    }
                    const ServiceRate rate(ServiceKind::DeclaredValue, RateMode::Percentage,
                                           Money{}, factor, cap);
                    REQUIRE(rate.calculateSurcharge(Money(basis), step) == Money(expected));
                }
            }
        }
    }
}
TEST_CASE("ServiceRate avoids intermediate overflow even with a finite cap", "[service-rate]") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    REQUIRE(detail::scaledCharge(Money(maximum), 1) == Money(922337203685478));
    REQUIRE(detail::scaledCharge(Money(1), maximum) == Money(922337203685478));
    REQUIRE(detail::scaledCharge(Money(maximum), 10000) == Money(maximum));
    REQUIRE(detail::scaledCharge(Money(maximum), maximum, 10, Money(105)) == Money(105));
    REQUIRE(detail::scaledCharge(Money(maximum), 10001, 1, Money(maximum)) == Money(maximum));
    REQUIRE_THROWS_AS(detail::scaledCharge(Money(maximum), maximum), InvalidAmountException);
    REQUIRE_THROWS_AS(detail::scaledCharge(Money(maximum / 2 + 1), 20000), InvalidAmountException);
    REQUIRE_THROWS_AS(detail::roundCharge(Money(maximum), 2), InvalidAmountException);
    REQUIRE_THROWS_AS(detail::scaledCharge(Money(1), -1), std::invalid_argument);
    REQUIRE_THROWS_AS(detail::roundCharge(Money{}, 0), std::invalid_argument);
    REQUIRE(detail::scaledCharge(Money{}, maximum) == Money{});
}
TEST_CASE("ServiceRate validates its product and selected option", "[service-rate]") {
    const ServiceRate registration(ServiceKind::Registration, RateMode::Fixed, Money(1), 0);
    REQUIRE_NOTHROW(registration.validateApplicability(ShipmentKind::Letter,
                                                       DeliveryOptions(true, Money{}, true)));
    REQUIRE_THROWS_AS(registration.validateApplicability(ShipmentKind::Letter, DeliveryOptions{}),
                      IncompatibleServiceException);
    const ServiceRate courier(ServiceKind::CourierDelivery, RateMode::Fixed, Money(1), 0);
    REQUIRE_NOTHROW(courier.validateApplicability(ShipmentKind::Parcel,
                                                  DeliveryOptions(false, Money{}, false, true)));
    REQUIRE_THROWS_AS(courier.validateApplicability(ShipmentKind::Letter,
                                                    DeliveryOptions(false, Money{}, false, true)),
                      IncompatibleServiceException);
    REQUIRE_THROWS_AS(
        registration.validateApplicability(static_cast<ShipmentKind>(99), DeliveryOptions{}),
        IncompatibleServiceException);
}
TEST_CASE("ServiceRate rejects inconsistent configuration", "[service-rate]") {
    REQUIRE_THROWS_AS(ServiceRate(ServiceKind::BasePostage, RateMode::Fixed, Money{}, 0),
                      IncompatibleServiceException);
    REQUIRE_THROWS_AS(ServiceRate(static_cast<ServiceKind>(-1), RateMode::Fixed, Money{}, 0),
                      IncompatibleServiceException);
    REQUIRE_THROWS_AS(ServiceRate(ServiceKind::Registration, static_cast<RateMode>(99), Money{}, 0),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(ServiceRate(ServiceKind::Registration, RateMode::Fixed, Money{}, 1),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(ServiceRate(ServiceKind::Registration, RateMode::Percentage, Money(1), 0),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(ServiceRate(ServiceKind::Registration, RateMode::Percentage, Money{}, -1),
                      std::invalid_argument);
}
