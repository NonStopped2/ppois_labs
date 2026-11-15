#include "../TariffFixtures.hpp"
#include "post/exceptions/TariffNotFoundException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
namespace {
TariffPlan custom(std::vector<TariffZone> zones, std::vector<WeightBand> bands,
                  std::vector<ServiceRate> rates = {}) {
    return TariffPlan("P", "План", test::moment(10), test::moment(20), std::move(zones),
                      std::move(bands), std::move(rates));
}
} // namespace
TEST_CASE("TariffPlan resolves references within inclusive date boundaries", "[tariff-plan]") {
    const auto plan = test::plan();
    REQUIRE(plan.id() == "plan");
    REQUIRE(plan.name() == "Учебный тариф");
    REQUIRE(plan.validFrom() == test::moment(0));
    REQUIRE(plan.validUntil() == test::moment(1000));
    REQUIRE(plan.zones().size() == 1);
    REQUIRE(plan.weightBands().size() == 1);
    REQUIRE(plan.serviceRates().size() == 5);
    REQUIRE_NOTHROW(plan.validateStructure());
    const auto selection = plan.resolveRates(test::customer().address(), 20, test::moment(1000));
    REQUIRE(&selection.first.get() == &plan.zones().front());
    REQUIRE(&selection.second.get() == &plan.weightBands().front());
    REQUIRE(plan.serviceRate(ServiceKind::Registration).fixedCharge() == Money(50));
    REQUIRE_NOTHROW(plan.resolveRates(test::customer().address(), 1, test::moment(0)));
}
TEST_CASE("TariffPlan rejects missing coverage weight date and service", "[tariff-plan]") {
    const auto plan = test::plan();
    REQUIRE_THROWS_AS(
        plan.resolveRates(test::customer("x", "230000").address(), 20, test::moment(10)),
        TariffNotFoundException);
    REQUIRE_THROWS_AS(plan.resolveRates(test::customer().address(), 1001, test::moment(10)),
                      TariffNotFoundException);
    REQUIRE_THROWS_AS(plan.resolveRates(test::customer().address(), 20, test::moment(1001)),
                      TariffNotFoundException);
    REQUIRE_THROWS_AS(plan.resolveRates(test::customer().address(), 0, test::moment(10)),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(plan.resolveRates(test::customer().address(), 20, test::moment(-1)),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(plan.serviceRate(ServiceKind::BasePostage), TariffNotFoundException);
    const auto later = custom(plan.zones(), plan.weightBands());
    REQUIRE_THROWS_AS(later.resolveRates(test::customer().address(), 20, test::moment(9)),
                      TariffNotFoundException);
}
TEST_CASE("TariffPlan rejects conflicts but allows gaps and adjacent weight bands",
          "[tariff-plan]") {
    const TariffZone zone("z", "z", {"22"}, 10000);
    const WeightBand band(0, 100, Money(1), 10, Money{});
    REQUIRE_THROWS_AS(custom({}, {band}), std::invalid_argument);
    REQUIRE_THROWS_AS(custom({zone}, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(custom({zone, TariffZone("other", "x", {"220"}, 10000)}, {band}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(custom({zone, TariffZone("z", "x", {"23"}, 10000)}, {band}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(custom({zone}, {band, WeightBand(99, 200, Money{}, 10, Money{})}),
                      std::invalid_argument);
    const ServiceRate rate(ServiceKind::Registration, RateMode::Fixed, Money(1), 0);
    REQUIRE_THROWS_AS(custom({zone}, {band}, {rate, rate}), std::invalid_argument);
    REQUIRE_NOTHROW(custom({zone}, {band, WeightBand(100, 200, Money{}, 1, Money{})}));
    const auto gap = custom({zone}, {band, WeightBand(200, 300, Money{}, 1, Money{})});
    REQUIRE_THROWS_AS(gap.resolveRates(test::customer().address(), 150, test::moment(10)),
                      TariffNotFoundException);
}
TEST_CASE("TariffPlan owns snapshots and validates metadata", "[tariff-plan]") {
    auto zones = test::plan().zones();
    auto bands = test::plan().weightBands();
    const auto original = custom(zones, bands);
    zones.clear();
    bands.clear();
    REQUIRE(original.zones().size() == 1);
    auto copy = original;
    REQUIRE(copy == original);
    copy = test::plan("other");
    REQUIRE(copy != original);
    REQUIRE_THROWS_AS(TariffPlan("p", " ", test::moment(0), test::moment(1), original.zones(),
                                 original.weightBands()),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(TariffPlan("p", "p", test::moment(2), test::moment(1), original.zones(),
                                 original.weightBands()),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(TariffPlan("p", "p", test::moment(-1), test::moment(1), original.zones(),
                                 original.weightBands()),
                      std::invalid_argument);
}
