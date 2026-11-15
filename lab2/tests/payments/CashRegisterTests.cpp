#include "../DeliveryFixtures.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/InvalidAmountException.hpp"
#include "post/exceptions/PaymentException.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
using namespace post;
TEST_CASE("CashRegister rejects replayed IDs backdating and operations during a break",
          "[cash-register]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    auto invoice = test::invoice(registry.at(id));
    auto branch = test::branch();
    CashRegister cash("cash", "A");
    cash.open(Money(100), branch, "worker", test::moment(0));
    const auto payment = test::payment("p", 100);
    cash.acceptPayment(invoice, payment, registry.at(id), branch, "worker");
    REQUIRE_THROWS_AS(cash.acceptPayment(invoice, payment, registry.at(id), branch, "worker"),
                      PaymentException);
    const auto refund = test::refund("r", "p", 10, 11);
    cash.refundPayment(invoice, refund, branch, "worker");
    REQUIRE_THROWS_AS(cash.refundPayment(invoice, refund, branch, "worker"), PaymentException);
    const auto before = cash;
    const auto bill = invoice;
    REQUIRE_THROWS_AS(
        cash.refundPayment(invoice, test::refund("earlier", "p", 10, 10), branch, "worker"),
        PaymentException);
    branch.addShiftBreak("shift", test::moment(12), test::moment(14));
    REQUIRE_THROWS_AS(
        cash.refundPayment(invoice, test::refund("break", "p", 10, 12), branch, "worker"),
        DeliveryAuthorizationException);
    REQUIRE(cash == before);
    REQUIRE(invoice == bill);
}
TEST_CASE("CashRegister separates cash and card flows and closes only after reconciliation",
          "[cash-register]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    auto invoice = test::invoice(registry.at(id));
    const auto branch = test::branch();
    CashRegister cash(" cash ", " A ");
    REQUIRE(cash.id() == "cash");
    REQUIRE(cash.branchId() == "A");
    REQUIRE_FALSE(cash.isOpen());
    REQUIRE_FALSE(cash.openedAt());
    REQUIRE_FALSE(cash.closedAt());
    REQUIRE(cash.compensations().empty());
    REQUIRE_THROWS_AS(
        cash.acceptPayment(invoice, test::payment("p", 40), registry.at(id), branch, "worker"),
        PaymentException);
    cash.open(Money(0), branch, "worker", test::moment(0));
    REQUIRE(cash.openingBalance() == Money{});
    REQUIRE(cash.openedAt() == test::moment(0));
    cash.acceptPayment(invoice, test::payment("cash", 40), registry.at(id), branch, "worker");
    cash.acceptPayment(invoice,
                       Payment("card", "invoice", Money(60), PaymentMethod::Card, test::moment(11)),
                       registry.at(id), branch, "worker");
    REQUIRE(cash.cashBalance() == Money(40));
    REQUIRE(cash.payments().size() == 2);
    REQUIRE(invoice.status() == InvoiceStatus::Paid);
    cash.refundPayment(invoice, test::refund("r1", "cash", 20, 12), branch, "worker");
    cash.refundPayment(invoice, test::refund("r2", "card", 30, 13), branch, "worker");
    REQUIRE(cash.cashBalance() == Money(20));
    REQUIRE(cash.refunds().size() == 2);
    REQUIRE(invoice.amountDue() == Money(50));
    cash.acceptPayment(
        invoice, Payment("again", "invoice", Money(50), PaymentMethod::Card, test::moment(14)),
        registry.at(id), branch, "worker");
    REQUIRE(cash.latestActivityAt() == test::moment(14));
    REQUIRE(cash.reconcile(Money(20)));
    REQUIRE_FALSE(cash.reconcile(Money(19)));
    const auto before = cash;
    REQUIRE_THROWS_AS(cash.close(Money(19), branch, "worker", test::moment(20)), PaymentException);
    REQUIRE(cash == before);
    cash.close(Money(20), branch, "worker", test::moment(20));
    REQUIRE_FALSE(cash.isOpen());
    REQUIRE(cash.closedAt() == test::moment(20));
    REQUIRE_THROWS_AS(cash.open(Money(20), branch, "worker", test::moment(19)), PaymentException);
    auto copy = cash;
    copy.open(Money(100), branch, "worker", test::moment(21));
    REQUIRE(copy != cash);
    REQUIRE(copy.payments().empty());
    REQUIRE(copy.cashBalance() == Money(100));
}
TEST_CASE("CashRegister rolls back invoice and journal on stale shipment or cash overflow",
          "[cash-register]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    auto invoice = test::invoice(registry.at(id));
    const auto branch = test::branch();
    CashRegister cash("c", "A");
    cash.open(Money(0), branch, "worker", test::moment(0));
    registry.at(id).revise(21, DeliveryOptions{});
    const auto before = cash;
    const auto invoiceBefore = invoice;
    REQUIRE_THROWS_AS(
        cash.acceptPayment(invoice, test::payment("p", 100), registry.at(id), branch, "worker"),
        PaymentException);
    REQUIRE(cash == before);
    REQUIRE(invoice == invoiceBefore);
    auto fresh = test::invoice(registry.at(id));
    CashRegister full("full", "A");
    full.open(Money(std::numeric_limits<std::int64_t>::max()), branch, "worker", test::moment(0));
    const auto fullBefore = full;
    const auto billBefore = fresh;
    REQUIRE_THROWS_AS(
        full.acceptPayment(fresh, test::payment("p", 1), registry.at(id), branch, "worker"),
        InvalidAmountException);
    REQUIRE(full == fullBefore);
    REQUIRE(fresh == billBefore);
}
TEST_CASE("CashRegister rejects insufficient refund funds duplicated operations and foreign branch",
          "[cash-register]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerLetter(registry);
    auto invoice = test::paidInvoice(registry, id);
    const auto branch = test::branch();
    CashRegister cash("c", "A");
    cash.open(Money(0), branch, "worker", test::moment(0));
    const auto before = cash;
    const auto billBefore = invoice;
    REQUIRE_THROWS_AS(
        cash.refundPayment(invoice, test::refund("r", "payment", 100, 11), branch, "worker"),
        PaymentException);
    REQUIRE(cash == before);
    REQUIRE(invoice == billBefore);
    REQUIRE_THROWS_AS(cash.open(Money(0), branch, "worker", test::moment(1)), PaymentException);
    REQUIRE_THROWS_AS(CashRegister("other", "B").open(Money(0), branch, "worker", test::moment(1)),
                      PaymentException);
}
TEST_CASE("CashRegister compensation is atomic and paid exactly once after approval",
          "[cash-register]") {
    ShipmentRegistry registry("BY");
    auto branch = test::branch();
    const auto id = test::registerCargo(registry);
    test::accept(branch, registry, id);
    const auto search = test::confirmLost(registry, branch, id);
    CompensationClaim claim("claim");
    claim.submit(registry.at(id), registry.at(id).sender(), Money(100), CompensationReason::Lost,
                 {"proof"}, test::moment(24));
    claim.approve(registry.at(id), branch, "worker", Money(100), Money(100), test::moment(25),
                  &search);
    CashRegister cash("cash", "A");
    cash.open(Money(99), branch, "worker", test::moment(0));
    const auto before = cash;
    const auto claimBefore = claim;
    REQUIRE_THROWS_AS(cash.payCompensation(claim, branch, "worker", test::moment(26)),
                      PaymentException);
    REQUIRE(cash == before);
    REQUIRE(claim == claimBefore);
    cash.close(Money(99), branch, "worker", test::moment(26));
    cash.open(Money(200), branch, "worker", test::moment(27));
    cash.payCompensation(claim, branch, "worker", test::moment(28));
    REQUIRE(claim.status() == CompensationStatus::Paid);
    REQUIRE(cash.cashBalance() == Money(100));
    REQUIRE(cash.compensations().at("claim").first == Money(100));
    REQUIRE(cash.latestActivityAt() == test::moment(28));
    auto oldSnapshot = claimBefore;
    REQUIRE_THROWS_AS(cash.payCompensation(oldSnapshot, branch, "worker", test::moment(29)),
                      PaymentException);
    REQUIRE(oldSnapshot == claimBefore);
}
