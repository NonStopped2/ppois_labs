#include "post/exceptions/InvalidAmountException.hpp"
#include "post/exceptions/TariffNotFoundException.hpp"
#include "post/tariffs/WeightBand.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace post;
TEST_CASE("WeightBand includes its upper bound and prices started additional steps",
          "[weight-band]") {
    const WeightBand band(100, 600, Money(100), 50, Money(25));
    REQUIRE(band.lowerExclusiveGrams() == 100);
    REQUIRE(band.upperInclusiveGrams() == 600);
    REQUIRE(band.basePrice() == Money(100));
    REQUIRE(band.stepGrams() == 50);
    REQUIRE(band.stepPrice() == Money(25));
    REQUIRE_FALSE(band.contains(100));
    REQUIRE(band.contains(101));
    REQUIRE(band.contains(600));
    REQUIRE_FALSE(band.contains(601));
    for (std::int64_t weight = 101; weight <= 600; ++weight) {
        const auto paidSteps = (weight - 100 + 49) / 50;
        REQUIRE(band.priceFor(weight) == Money(100 + (paidSteps - 1) * 25));
    }
    REQUIRE_THROWS_AS(band.priceFor(100), TariffNotFoundException);
    REQUIRE_THROWS_AS(band.priceFor(601), PostalException);
}
TEST_CASE("WeightBand overlap uses half-open interval semantics", "[weight-band]") {
    const WeightBand first(0, 100, Money{}, 10, Money{});
    const WeightBand adjacent(100, 200, Money{}, 10, Money{});
    const WeightBand overlapping(99, 101, Money{}, 10, Money{});
    REQUIRE_FALSE(first.overlaps(adjacent));
    REQUIRE(first.overlaps(overlapping));
    REQUIRE(overlapping.overlaps(first));
    REQUIRE(first.overlaps(first));
    REQUIRE(first == first);
    REQUIRE(first != adjacent);
    REQUIRE(first.priceFor(100) == Money{});
}
TEST_CASE("WeightBand validates settings and extreme arithmetic", "[weight-band]") {
    REQUIRE_THROWS_AS(WeightBand(-1, 10, Money{}, 1, Money{}), std::invalid_argument);
    REQUIRE_THROWS_AS(WeightBand(10, 10, Money{}, 1, Money{}), std::invalid_argument);
    REQUIRE_THROWS_AS(WeightBand(10, 9, Money{}, 1, Money{}), std::invalid_argument);
    REQUIRE_THROWS_AS(WeightBand(0, 10, Money{}, 0, Money{}), std::invalid_argument);
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    REQUIRE(WeightBand(0, maximum, Money{}, 1, Money(1)).priceFor(maximum) == Money(maximum - 1));
    REQUIRE_THROWS_AS(WeightBand(0, 3, Money(maximum), 1, Money(1)).priceFor(2),
                      InvalidAmountException);
    REQUIRE_THROWS_AS(WeightBand(0, 3, Money{}, 1, Money(maximum)).priceFor(3),
                      InvalidAmountException);
}
