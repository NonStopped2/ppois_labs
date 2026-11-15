#include "../TariffFixtures.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace post;
TEST_CASE("TariffZone covers string prefixes and preserves leading zeroes", "[tariff-zone]") {
    const TariffZone zone(" zone ", " Учебная   зона ", {" 00 ", "220000"}, 15000);
    REQUIRE(zone.id() == "zone");
    REQUIRE(zone.name() == "Учебная зона");
    REQUIRE(zone.postalPrefixes() == std::vector<std::string>{"00", "220000"});
    REQUIRE(zone.priceFactorBasisPoints() == 15000);
    REQUIRE_NOTHROW(zone.validateCoverage());
    REQUIRE(zone.covers(test::customer("x", "000001").address()));
    REQUIRE(zone.covers(test::customer("x", "220000").address()));
    REQUIRE_FALSE(zone.covers(test::customer("x", "220001").address()));
    REQUIRE_NOTHROW(TariffZone("x", "x", {"1"}, std::numeric_limits<std::int64_t>::max()));
}
TEST_CASE("TariffZone detects nested, equal and disjoint coverage symmetrically", "[tariff-zone]") {
    const TariffZone first("a", "a", {"22", "00"}, 10000);
    const TariffZone nested("b", "b", {"220"}, 10000);
    const TariffZone separate("c", "c", {"23"}, 10000);
    REQUIRE(first.overlaps(nested));
    REQUIRE(nested.overlaps(first));
    REQUIRE(first.overlaps(first));
    REQUIRE_FALSE(first.overlaps(separate));
    auto copy = first;
    REQUIRE(copy == first);
    copy = separate;
    REQUIRE(copy != first);
    REQUIRE(first.postalPrefixes().size() == 2);
}
TEST_CASE("TariffZone rejects malformed and redundant prefixes", "[tariff-zone]") {
    for (const auto& prefixes : std::vector<std::vector<std::string>>{
             {}, {""}, {" "}, {"1234567"}, {"2a"}, {"2 2"}, {"22", "220"}, {"22", "22"}}) {
        REQUIRE_THROWS_AS(TariffZone("a", "a", prefixes, 10000), std::invalid_argument);
    }
    REQUIRE_THROWS_AS(TariffZone("a", " ", {"22"}, 10000), std::invalid_argument);
    REQUIRE_THROWS_AS(TariffZone("a", "a", {"22"}, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(TariffZone("a", "a", {"22"}, -1), std::invalid_argument);
}
