#include "../PaymentFixtures.hpp"
#include "post/exceptions/PaymentException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("Payment stores normalized identity, amount, method and explicit time", "[payment]") {
    const Payment payment(" pay ", " invoice ", Money(25), PaymentMethod::Card, test::moment(12));
    REQUIRE(payment.id() == "pay");
    REQUIRE(payment.invoiceId() == "invoice");
    REQUIRE(payment.amount() == Money(25));
    REQUIRE(payment.method() == PaymentMethod::Card);
    REQUIRE(payment.paidAt() == test::moment(12));
    auto copy = payment;
    REQUIRE(copy == payment);
    copy = test::payment("other", 1);
    REQUIRE(copy != payment);
}
TEST_CASE("Payment rejects zero values unknown methods and malformed metadata", "[payment]") {
    REQUIRE_THROWS_AS(test::payment("p", 0), PaymentException);
    REQUIRE_THROWS_AS(
        Payment("p", "invoice", Money(1), static_cast<PaymentMethod>(99), test::moment(10)),
        PaymentException);
    REQUIRE_THROWS_AS(test::payment("bad id", 1), std::invalid_argument);
    REQUIRE_THROWS_AS(test::payment("p", 1, -1), std::invalid_argument);
}
TEST_CASE("Payment validates invoice ownership, inclusive dates and balance", "[payment]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    auto invoice = test::invoice(registry.at(id));
    REQUIRE_NOTHROW(test::payment("p", 100, 10).validateFor(invoice));
    REQUIRE_NOTHROW(test::payment("p", 100, 20).validateFor(invoice));
    REQUIRE_THROWS_AS(test::payment("p", 1, 9).validateFor(invoice), PaymentException);
    REQUIRE_THROWS_AS(test::payment("p", 1, 21).validateFor(invoice), PaymentException);
    REQUIRE_THROWS_AS(test::payment("p", 101).validateFor(invoice), PaymentException);
    REQUIRE_THROWS_AS(
        Payment("p", "other", Money(1), PaymentMethod::Cash, test::moment(10)).validateFor(invoice),
        PaymentException);
    invoice.cancel();
    REQUIRE_THROWS_AS(test::payment("p", 1).validateFor(invoice), PaymentException);
}
TEST_CASE("Payment rejects duplicate identifiers and dates preceding latest activity",
          "[payment]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    auto invoice = test::invoice(registry.at(id));
    invoice.registerPayment(test::payment("p", 10, 12), registry.at(id));
    REQUIRE_THROWS_AS(test::payment("p", 1, 13).validateFor(invoice), PaymentException);
    REQUIRE_THROWS_AS(test::payment("other", 1, 11).validateFor(invoice), PaymentException);
    REQUIRE_NOTHROW(test::payment("other", 90, 12).validateFor(invoice));
}
