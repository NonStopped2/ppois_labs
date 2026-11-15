#include "post/exceptions/InvalidAmountException.hpp"
#include "post/exceptions/PaymentException.hpp"
#include "post/payments/InvoiceLine.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace post;
TEST_CASE("InvoiceLine calculates checked totals and owns a normalized description",
          "[invoice-line]") {
    std::string description = " Доставка   письма ";
    const InvoiceLine line(ServiceKind::BasePostage, description, 3, Money(125));
    description = "Изменено";
    REQUIRE(line.service() == ServiceKind::BasePostage);
    REQUIRE(line.description() == "Доставка письма");
    REQUIRE(line.quantity() == 3);
    REQUIRE(line.unitPrice() == Money(125));
    REQUIRE(line.lineTotal() == Money(375));
    auto copy = line;
    REQUIRE(copy == line);
    copy = InvoiceLine(ServiceKind::Registration, "Регистрация", 1, Money{});
    REQUIRE(copy != line);
    REQUIRE(copy.lineTotal() == Money{});
}
TEST_CASE("InvoiceLine rejects malformed rows and safely detects overflow", "[invoice-line]") {
    REQUIRE_THROWS_AS(InvoiceLine(static_cast<ServiceKind>(-1), "x", 1, Money{}), PaymentException);
    REQUIRE_THROWS_AS(InvoiceLine(static_cast<ServiceKind>(99), "x", 1, Money{}), PostalException);
    REQUIRE_THROWS_AS(InvoiceLine(ServiceKind::Registration, " ", 1, Money{}), PaymentException);
    REQUIRE_THROWS_AS(InvoiceLine(ServiceKind::Registration, "x", 0, Money{}), PaymentException);
    const InvoiceLine large(ServiceKind::BasePostage, "x", 2,
                            Money(std::numeric_limits<std::int64_t>::max()));
    REQUIRE_THROWS_AS(large.lineTotal(), InvalidAmountException);
    REQUIRE(large.quantity() == 2);
    REQUIRE(large.unitPrice().kopecks() == std::numeric_limits<std::int64_t>::max());
}
