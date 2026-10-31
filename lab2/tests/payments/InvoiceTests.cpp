#include "../PaymentFixtures.hpp"
#include "post/exceptions/PaymentException.hpp"
#include "post/tariffs/PostageCalculator.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace post;
TEST_CASE("Invoice registers partial and full payments and owns its quote", "[invoice]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    auto invoice = test::invoice(registry.at(id));
    REQUIRE(invoice.id() == "invoice");
    REQUIRE(invoice.quote().total() == Money(100));
    REQUIRE(invoice.issuedAt() == test::moment(10));
    REQUIRE(invoice.expiresAt() == test::moment(20));
    REQUIRE(invoice.latestActivityAt() == test::moment(10));
    REQUIRE(invoice.status() == InvoiceStatus::Open);
    invoice.registerPayment(test::payment("p1", 30), registry.at(id));
    REQUIRE(invoice.amountDue() == Money(70));
    REQUIRE(invoice.status() == InvoiceStatus::PartiallyPaid);
    invoice.registerPayment(
        Payment("p2", "invoice", Money(70), PaymentMethod::Card, test::moment(20)),
        registry.at(id));
    REQUIRE(invoice.status() == InvoiceStatus::Paid);
    REQUIRE(invoice.amountDue() == Money{});
    REQUIRE(invoice.payments().size() == 2);
    REQUIRE(invoice.refunds().empty());
    REQUIRE(invoice.latestActivityAt() == test::moment(20));
    REQUIRE_THROWS_AS(invoice.cancel(), PaymentException);
}
TEST_CASE("Invoice payment rejection leaves the entire object unchanged", "[invoice]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    const auto other = test::registerLetter(registry);
    auto invoice = test::invoice(registry.at(id));
    const auto before = invoice;
    REQUIRE_THROWS_AS(invoice.registerPayment(test::payment("p", 101), registry.at(id)),
                      PaymentException);
    REQUIRE(invoice == before);
    REQUIRE_THROWS_AS(invoice.registerPayment(test::payment("p", 10), registry.at(other)),
                      PaymentException);
    REQUIRE(invoice == before);
    registry.at(id).revise(30, DeliveryOptions{});
    REQUIRE_THROWS_AS(invoice.registerPayment(test::payment("p", 10), registry.at(id)),
                      PaymentException);
    REQUIRE(invoice == before);
    auto copy = before;
    copy.cancel();
    REQUIRE(copy != before);
    REQUIRE(invoice == before);
}
TEST_CASE("Invoice refunds reopen debt and fully returned funds permit cancellation", "[invoice]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    auto invoice = test::invoice(registry.at(id));
    invoice.registerPayment(test::payment("p1", 40), registry.at(id));
    invoice.registerPayment(test::payment("p2", 60, 11), registry.at(id));
    invoice.registerRefund(test::refund("r1", "p1", 10, 12));
    REQUIRE(invoice.amountDue() == Money(10));
    REQUIRE(invoice.status() == InvoiceStatus::PartiallyRefunded);
    REQUIRE_THROWS_AS(invoice.cancel(), PaymentException);
    invoice.registerRefund(test::refund("r2", "p1", 30, 13));
    invoice.registerRefund(test::refund("r3", "p2", 60, 25));
    REQUIRE(invoice.refunds().size() == 3);
    REQUIRE(invoice.amountDue() == Money(100));
    REQUIRE(invoice.status() == InvoiceStatus::Refunded);
    REQUIRE(invoice.latestActivityAt() == test::moment(25));
    invoice.cancel();
    REQUIRE(invoice.status() == InvoiceStatus::Cancelled);
    REQUIRE(invoice.amountDue() == Money{});
    REQUIRE_THROWS_AS(invoice.cancel(), PaymentException);
    REQUIRE_THROWS_AS(invoice.registerRefund(test::refund("r4", "p1", 1, 26)), PaymentException);
}
TEST_CASE("Invoice rejects duplicate foreign excessive and backdated refunds atomically",
          "[invoice]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    auto invoice = test::invoice(registry.at(id));
    invoice.registerPayment(test::payment("p1", 40), registry.at(id));
    invoice.registerPayment(test::payment("p2", 60), registry.at(id));
    invoice.registerRefund(test::refund("r1", "p1", 10, 12));
    const auto before = invoice;
    const auto bad =
        std::vector{test::refund("r1", "p2", 1, 13), test::refund("r2", "unknown", 1, 13),
                    test::refund("r2", "p1", 31, 13), test::refund("r2", "p2", 1, 11),
                    Refund("r2", "p1", "other", Money(1), test::moment(13), "x")};
    for (const auto& refund : bad) {
        REQUIRE_THROWS_AS(invoice.registerRefund(refund), PaymentException);
        REQUIRE(invoice == before);
    }
    REQUIRE_THROWS_AS(invoice.registerPayment(test::payment("p3", 1, 11), registry.at(id)),
                      PaymentException);
    REQUIRE(invoice == before);
}
TEST_CASE("Invoice supports repayment after refund without overflowing historical turnover",
          "[invoice]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    auto invoice = test::invoice(registry.at(id), Money(maximum));
    for (int cycle = 0; cycle < 3; ++cycle) {
        const auto paid = "p" + std::to_string(cycle);
        invoice.registerPayment(test::payment(paid, maximum, 10 + cycle * 2), registry.at(id));
        REQUIRE(invoice.amountDue() == Money{});
        invoice.registerRefund(
            test::refund("r" + std::to_string(cycle), paid, maximum, 11 + cycle * 2));
        REQUIRE(invoice.amountDue() == Money(maximum));
        REQUIRE(invoice.status() == InvoiceStatus::Refunded);
    }
    invoice.registerPayment(test::payment("p-final", maximum, 16), registry.at(id));
    REQUIRE(invoice.status() == InvoiceStatus::Paid);
    REQUIRE(invoice.amountDue() == Money{});
}
TEST_CASE("Invoice permits partial repayment and cancellation without retained funds",
          "[invoice]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    auto invoice = test::invoice(registry.at(id));
    invoice.registerPayment(test::payment("p", 30), registry.at(id));
    invoice.registerRefund(test::refund("r", "p", 30, 11));
    invoice.registerPayment(test::payment("again", 20, 12), registry.at(id));
    REQUIRE(invoice.status() == InvoiceStatus::PartiallyRefunded);
    REQUIRE(invoice.amountDue() == Money(80));
    auto empty = test::invoice(registry.at(id));
    empty.cancel();
    REQUIRE_THROWS_AS(empty.registerPayment(test::payment("p", 1), registry.at(id)),
                      PaymentException);
    auto free = test::invoice(registry.at(id), Money{});
    REQUIRE(free.status() == InvoiceStatus::Paid);
    REQUIRE(free.amountDue() == Money{});
    free.cancel();
    REQUIRE(free.status() == InvoiceStatus::Cancelled);
}
TEST_CASE("Invoice validates its entire period against quote boundaries", "[invoice]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    const auto quote = test::quote(registry.at(id));
    REQUIRE_THROWS_AS(Invoice("i", quote, test::moment(9), test::moment(20)), PaymentException);
    REQUIRE_THROWS_AS(Invoice("i", quote, test::moment(10), test::moment(21)), PaymentException);
    REQUIRE_THROWS_AS(Invoice("i", quote, test::moment(15), test::moment(14)), PaymentException);
    REQUIRE_THROWS_AS(Invoice("i", quote, test::moment(-1), test::moment(20)),
                      std::invalid_argument);
    REQUIRE_NOTHROW(Invoice("i", quote, test::moment(20), test::moment(20)));
}
TEST_CASE("Pricing payment acceptance and history compose without hidden state mutation",
          "[invoice][integration]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    auto& shipment = registry.at(id);
    const PostageCalculator calculator({test::plan()}, 1, std::chrono::seconds{10});
    const auto quote = calculator.quote(shipment, "plan", "q", test::moment(10));
    Invoice invoice("invoice", quote, test::moment(10), test::moment(20));
    invoice.registerPayment(test::payment("paid", 150, 10), shipment);
    REQUIRE(invoice.status() == InvoiceStatus::Paid);
    REQUIRE(shipment.status() == ShipmentStatus::Draft);
    shipment.applyTrackingEvent(
        TrackingEvent(1, TrackingEventKind::Accepted, test::moment(11), "A"));
    REQUIRE(shipment.status() == ShipmentStatus::Accepted);
    REQUIRE(shipment.history().events().size() == 1);
    REQUIRE(invoice.amountDue() == Money{});
}
