#include "../TariffFixtures.hpp"
#include "post/exceptions/InvalidAmountException.hpp"
#include "post/exceptions/PaymentException.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace post;
namespace {
InvoiceLine row(ServiceKind kind, std::int64_t price = 100) {
    return InvoiceLine(kind, "Услуга", 1, Money(price));
}
PostageQuote withLines(std::vector<InvoiceLine> lines) {
    return PostageQuote("q", "BY-1", 0, "p", test::moment(10), test::moment(20), std::move(lines));
}
} // namespace
TEST_CASE("PostageQuote owns line snapshots and exposes calculation identity", "[postage-quote]") {
    auto lines =
        std::vector{row(ServiceKind::BasePostage, 100), row(ServiceKind::Registration, 50)};
    const auto quote = withLines(lines);
    lines.clear();
    REQUIRE(quote.id() == "q");
    REQUIRE(quote.shipmentTrackingNumber() == "BY-1");
    REQUIRE(quote.shipmentRevision() == 0);
    REQUIRE(quote.tariffPlanId() == "p");
    REQUIRE(quote.createdAt() == test::moment(10));
    REQUIRE(quote.expiresAt() == test::moment(20));
    REQUIRE(quote.lines().size() == 2);
    REQUIRE(quote.total() == Money(150));
    auto copy = quote;
    REQUIRE(copy == quote);
    copy = withLines({row(ServiceKind::BasePostage, 1)});
    REQUIRE(copy != quote);
    REQUIRE(quote.total() == Money(150));
}
TEST_CASE("PostageQuote validates time, shipment identity, state and revision", "[postage-quote]") {
    ShipmentRegistry registry("BY");
    const auto first = test::registerLetter(registry);
    const auto second = test::registerLetter(registry);
    auto& shipment = registry.at(first);
    const auto quote = test::quote(shipment);
    REQUIRE_NOTHROW(quote.validateFor(shipment, test::moment(10)));
    REQUIRE_NOTHROW(quote.validateFor(shipment, test::moment(20)));
    REQUIRE_THROWS_AS(quote.validateFor(shipment, test::moment(9)), PaymentException);
    REQUIRE_THROWS_AS(quote.validateFor(shipment, test::moment(21)), PaymentException);
    REQUIRE_THROWS_AS(quote.validateFor(registry.at(second), test::moment(10)), PaymentException);
    REQUIRE_THROWS_AS(quote.validateFor(shipment, test::moment(-1)), std::invalid_argument);
    shipment.revise(30, DeliveryOptions{});
    REQUIRE_THROWS_AS(quote.validateFor(shipment, test::moment(10)), PaymentException);
    const auto fresh = test::quote(shipment);
    test::advance(shipment, TrackingEventKind::Accepted);
    REQUIRE_THROWS_AS(fresh.validateFor(shipment, test::moment(10)), PaymentException);
}
TEST_CASE("PostageQuote checks required services without trusting a matching revision",
          "[postage-quote]") {
    auto prototype = test::letter();
    prototype.revise(20, DeliveryOptions(true, Money(1000), true, false, false, true));
    ShipmentRegistry registry("BY");
    const auto id = registry.registerShipment(prototype, 100, Dimensions(300, 200, 10), 10);
    const auto& shipment = registry.at(id);
    const auto make = [&](std::vector<InvoiceLine> lines) {
        return PostageQuote("q", id, shipment.revision(), "p", test::moment(10), test::moment(20),
                            std::move(lines));
    };
    const auto correct =
        make({row(ServiceKind::BasePostage), row(ServiceKind::Registration),
              row(ServiceKind::DeclaredValue), row(ServiceKind::DeliveryNotification)});
    REQUIRE_NOTHROW(correct.validateFor(shipment, test::moment(10)));
    REQUIRE_THROWS_AS(make({row(ServiceKind::BasePostage)}).validateFor(shipment, test::moment(10)),
                      PaymentException);
    const auto wrong = make({row(ServiceKind::BasePostage), row(ServiceKind::Registration),
                             row(ServiceKind::DeclaredValue), row(ServiceKind::FragileHandling)});
    REQUIRE_THROWS_AS(wrong.validateFor(shipment, test::moment(10)), PaymentException);
}
TEST_CASE("PostageQuote rejects ambiguous rows bad metadata and overflow", "[postage-quote]") {
    REQUIRE_THROWS_AS(withLines({}), PaymentException);
    REQUIRE_THROWS_AS(withLines({row(ServiceKind::Registration)}), PaymentException);
    REQUIRE_THROWS_AS(withLines({row(ServiceKind::BasePostage), row(ServiceKind::BasePostage)}),
                      PaymentException);
    REQUIRE_THROWS_AS(PostageQuote("q", "BY-1", -1, "p", test::moment(10), test::moment(20),
                                   {row(ServiceKind::BasePostage)}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(PostageQuote("q", "BY-1", 0, "p", test::moment(20), test::moment(10),
                                   {row(ServiceKind::BasePostage)}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(PostageQuote("q", "BY-1", 0, "p", test::moment(-1), test::moment(10),
                                   {row(ServiceKind::BasePostage)}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        withLines({row(ServiceKind::BasePostage, std::numeric_limits<std::int64_t>::max()),
                   row(ServiceKind::Registration, 1)}),
        InvalidAmountException);
}
