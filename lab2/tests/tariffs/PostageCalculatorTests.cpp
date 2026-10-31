#include "../TariffFixtures.hpp"
#include "post/exceptions/InvalidAmountException.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"
#include "post/exceptions/TariffNotFoundException.hpp"
#include "post/tariffs/PostageCalculator.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace post;
TEST_CASE("PostageCalculator explains all six price components without duplicate identity charge",
          "[postage-calculator]") {
    const DeliveryOptions options(true, Money(12345), true, true, true, true);
    Parcel prototype(test::customer("s"), test::customer("r"), 500, options,
                     Dimensions(200, 100, 50), PackagingKind::Box);
    prototype.addItem(ContentItem("Ваза", 1, 100, Money(12345), ContentCategory::FragileGoods));
    prototype.seal("seal");
    ShipmentRegistry registry("BY");
    const auto id = registry.registerShipment(prototype, 1000, Dimensions(300, 200, 100), 10);
    const PostageCalculator calculator({test::plan()}, 10, std::chrono::seconds{60});
    const auto quote = calculator.quote(registry.at(id), "plan", "q", test::moment(10));
    REQUIRE(calculator.tariffPlans().size() == 1);
    REQUIRE(calculator.roundingStepKopecks() == 10);
    REQUIRE(calculator.quoteLifetime() == std::chrono::seconds{60});
    REQUIRE(quote.lines().size() == 6);
    const std::vector<std::int64_t> expected{270, 50, 130, 100, 30, 20};
    for (std::size_t line = 0; line < expected.size(); ++line) {
        REQUIRE(quote.lines()[line].lineTotal() == Money(expected[line]));
    }
    REQUIRE(quote.total() == Money(600));
    REQUIRE(quote.expiresAt() == test::moment(70));
    REQUIRE_NOTHROW(quote.validateFor(registry.at(id), test::moment(10)));
    REQUIRE(quote == calculator.quote(registry.at(id), "plan", "q", test::moment(10)));
}
TEST_CASE("PostageCalculator compares available plans by total then stable plan id",
          "[postage-calculator]") {
    auto prototype = test::letter();
    prototype.revise(20, DeliveryOptions(true, Money{}, true));
    ShipmentRegistry registry("BY");
    const auto id = registry.registerShipment(prototype, 1000, Dimensions(300, 200, 10), 10);
    const auto missingService =
        TariffPlan("missing-service", "Без услуги", test::moment(0), test::moment(1000),
                   test::plan().zones(), test::plan().weightBands());
    const PostageCalculator calculator(
        {test::plan("c", Money(200)), test::plan("b"), missingService, test::plan("a")}, 1,
        std::chrono::seconds{60});
    const auto quotes =
        calculator.compareAvailablePlans(registry.at(id), "compare", test::moment(10));
    REQUIRE(quotes.size() == 3);
    REQUIRE(quotes[0].tariffPlanId() == "a");
    REQUIRE(quotes[1].tariffPlanId() == "b");
    REQUIRE(quotes[2].tariffPlanId() == "c");
    REQUIRE(quotes[0].id() == "compare-a");
    REQUIRE(quotes[0].total() == Money(200));
    REQUIRE(quotes[2].total() == Money(350));
    REQUIRE(calculator.compareAvailablePlans(registry.at(id), "none", test::moment(1001)).empty());
    REQUIRE_THROWS_AS(calculator.quote(registry.at(id), "absent", "q", test::moment(10)),
                      TariffNotFoundException);
}
TEST_CASE("PostageCalculator clips quote life without overflowing absolute dates",
          "[postage-calculator]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    const PostageCalculator calculator({test::plan()}, 1, std::chrono::seconds{60});
    REQUIRE(calculator.quote(registry.at(id), "plan", "q", test::moment(990)).expiresAt() ==
            test::moment(1000));
    REQUIRE(calculator.quote(registry.at(id), "plan", "q", test::moment(1000)).expiresAt() ==
            test::moment(1000));
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const TariffPlan distant("far", "Дальний срок", test::moment(0), test::moment(maximum),
                             {TariffZone("z", "z", {"22"}, 10000)},
                             {WeightBand(0, 1000, Money(1), 1000, Money{})});
    const PostageCalculator longLife({distant}, 1, std::chrono::seconds::max());
    REQUIRE(longLife.quote(registry.at(id), "far", "q", test::moment(maximum - 1)).expiresAt() ==
            test::moment(maximum));
}
TEST_CASE("PostageCalculator rejects nonready shipments and invalid configuration",
          "[postage-calculator]") {
    REQUIRE_THROWS_AS(PostageCalculator({}, 1, std::chrono::seconds{1}), std::invalid_argument);
    REQUIRE_THROWS_AS(PostageCalculator({test::plan()}, 0, std::chrono::seconds{1}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(PostageCalculator({test::plan()}, 1, std::chrono::seconds{0}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(PostageCalculator({test::plan(), test::plan()}, 1, std::chrono::seconds{1}),
                      std::invalid_argument);
    const PostageCalculator calculator({test::plan()}, 1, std::chrono::seconds{1});
    REQUIRE_THROWS_AS(calculator.quote(test::letter(), "plan", "q", test::moment(10)),
                      InvalidShipmentStateException);
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    REQUIRE_THROWS_AS(calculator.quote(registry.at("BY-1"), "plan", "q", test::moment(10)),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(calculator.compareAvailablePlans(registry.at("BY-1"), "q", test::moment(-1)),
                      std::invalid_argument);
    const auto parcelId =
        registry.registerShipment(test::parcel(), 1000, Dimensions(500, 500, 500), 10);
    registry.at(parcelId).revise(1, DeliveryOptions{});
    REQUIRE_THROWS_AS(calculator.quote(registry.at(parcelId), "plan", "q", test::moment(10)),
                      ShipmentLimitExceededException);
}
TEST_CASE("PostageCalculator never hides arithmetic errors as unavailable plans",
          "[postage-calculator]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    const auto costly = test::plan("overflow", Money(std::numeric_limits<std::int64_t>::max()));
    const PostageCalculator calculator({test::plan(), costly}, 1, std::chrono::seconds{10});
    const auto before = registry.at(id).history();
    REQUIRE_THROWS_AS(calculator.quote(registry.at(id), "overflow", "q", test::moment(10)),
                      InvalidAmountException);
    REQUIRE_THROWS_AS(calculator.compareAvailablePlans(registry.at(id), "q", test::moment(10)),
                      InvalidAmountException);
    REQUIRE(registry.at(id).history() == before);
    REQUIRE(registry.at(id).revision() == 0);
}
