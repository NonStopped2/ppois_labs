#include "../PaymentFixtures.hpp"
#include "post/exceptions/PaymentException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("Refund owns an explained link to one invoice payment", "[refund]") {
    const Refund refund(" r ", " p ", " invoice ", Money(25), test::moment(12),
                        " Ошибка   оплаты ");
    REQUIRE(refund.id() == "r");
    REQUIRE(refund.paymentId() == "p");
    REQUIRE(refund.invoiceId() == "invoice");
    REQUIRE(refund.amount() == Money(25));
    REQUIRE(refund.createdAt() == test::moment(12));
    REQUIRE(refund.reason() == "Ошибка оплаты");
    auto copy = refund;
    REQUIRE(copy == refund);
    copy = test::refund("other", "p", 1);
    REQUIRE(copy != refund);
}
TEST_CASE("Refund checks remaining amount and allows the payment timestamp itself", "[refund]") {
    const auto payment = test::payment("p", 100, 10);
    REQUIRE_NOTHROW(test::refund("r", "p", 100, 10).validateAgainst(payment, Money{}));
    REQUIRE_NOTHROW(test::refund("r", "p", 40, 12).validateAgainst(payment, Money(60)));
    REQUIRE_THROWS_AS(test::refund("r", "p", 41, 12).validateAgainst(payment, Money(60)),
                      PaymentException);
    REQUIRE_THROWS_AS(test::refund("r", "p", 1, 12).validateAgainst(payment, Money(101)),
                      PaymentException);
    REQUIRE_THROWS_AS(test::refund("r", "p", 1, 12).validateAgainst(payment, Money(100)),
                      PaymentException);
}
TEST_CASE("Refund rejects mismatched payment invoice and early timestamps", "[refund]") {
    const auto payment = test::payment("p", 100, 10);
    REQUIRE_THROWS_AS(test::refund("r", "other", 1, 12).validateAgainst(payment, Money{}),
                      PaymentException);
    REQUIRE_THROWS_AS(Refund("r", "p", "other", Money(1), test::moment(12), "x")
                          .validateAgainst(payment, Money{}),
                      PaymentException);
    REQUIRE_THROWS_AS(test::refund("r", "p", 1, 9).validateAgainst(payment, Money{}),
                      PaymentException);
    REQUIRE(payment.amount() == Money(100));
}
TEST_CASE("Refund requires positive money, a reason and valid identity and time", "[refund]") {
    REQUIRE_THROWS_AS(test::refund("r", "p", 0), PaymentException);
    REQUIRE_THROWS_AS(Refund("r", "p", "invoice", Money(1), test::moment(10), " "),
                      PaymentException);
    REQUIRE_THROWS_AS(test::refund("bad id", "p", 1), std::invalid_argument);
    REQUIRE_THROWS_AS(test::refund("r", "p", 1, -1), std::invalid_argument);
}
