#include "../PurchaseFixtures.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/DuplicateOperationException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/common/PaymentRuleException.hpp"
#include "shop/payments/PaymentBook.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

TEST_CASE("Payment attempts have a single explained terminal outcome") {
    PaymentAttempt attempt("a", "o", Money(1), PaymentMethod::SimulatedCard, 10);
    REQUIRE(attempt.id() == "a");
    REQUIRE(attempt.orderId() == "o");
    REQUIRE(attempt.amount() == Money(1));
    REQUIRE(attempt.method() == PaymentMethod::SimulatedCard);
    REQUIRE(attempt.createdAt() == 10);
    REQUIRE(attempt.status() == AttemptStatus::Pending);
    REQUIRE(attempt.failureReason().empty());
    REQUIRE_THROWS_AS(Payment("p", attempt), PaymentRuleException);
    const auto original = attempt;
    REQUIRE_THROWS_AS(attempt.decline(" "), InvalidDomainDataException);
    REQUIRE(attempt == original);
    attempt.decline("Лимит учебной карты");
    REQUIRE(attempt.status() == AttemptStatus::Declined);
    REQUIRE(attempt.failureReason() == "Лимит учебной карты");
    REQUIRE_THROWS_AS(attempt.succeed(), PaymentRuleException);
    REQUIRE_THROWS_AS(attempt.decline("again"), PaymentRuleException);
    attempt = original;
    attempt.succeed();
    REQUIRE(attempt.status() == AttemptStatus::Succeeded);
    REQUIRE_THROWS_AS(attempt.succeed(), PaymentRuleException);
    REQUIRE_THROWS_AS(PaymentAttempt("a", "o", Money{}, PaymentMethod::SimulatedCard, 0),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(PaymentAttempt("a", "o", Money(1), static_cast<PaymentMethod>(99), 0),
                      InvalidDomainDataException);
}

TEST_CASE("Payment documents replay before phase checks and reject mismatched command reuse") {
    PaymentBook book;
    auto order = test::order();
    const auto declined = book.recordAttempt(
        "a1", "", order, Money(110), PaymentMethod::SimulatedCard, 11, false, "Отказ", "op1");
    REQUIRE_FALSE(declined.successful);
    REQUIRE_FALSE(declined.paymentId);
    REQUIRE(book.payments().empty());
    REQUIRE(book.attempts().size() == 1);
    REQUIRE(book.recordAttempt("ignored", "", order, Money(110), PaymentMethod::SimulatedCard, 12,
                               false, "Отказ", "op1") == declined);
    const auto before = book;
    REQUIRE_THROWS_AS(book.recordAttempt("a2", "p", order, Money(109), PaymentMethod::SimulatedCard,
                                         12, true, "", "op2"),
                      PaymentRuleException);
    REQUIRE_THROWS_AS(book.recordAttempt("a2", "p", order, Money(110), PaymentMethod::SimulatedCard,
                                         10, true, "", "op2"),
                      PaymentRuleException);
    REQUIRE_THROWS_AS(book.recordAttempt("a2", "p", order, Money(110), PaymentMethod::SimulatedCard,
                                         12, true, "", "op1"),
                      DuplicateOperationException);
    REQUIRE(book == before);
    const auto paid = book.recordAttempt("a2", "p", order, Money(110),
                                         PaymentMethod::SimulatedTransfer, 12, true, "", "op2");
    REQUIRE(paid.successful);
    REQUIRE(paid.paymentId == "p");
    order.confirmPayment(12, "op2", "p");
    REQUIRE(book.recordAttempt("ignored", "ignored", order, Money(110),
                               PaymentMethod::SimulatedTransfer, 13, true, "", "op2") == paid);
    REQUIRE(book.attempts().size() == 2);
    REQUIRE(book.operationIndex().size() == 2);
    const auto payment = book.payment("p");
    REQUIRE(payment.id() == "p");
    REQUIRE(payment.attemptId() == "a2");
    REQUIRE(payment.orderId() == "order");
    REQUIRE(payment.capturedAmount() == Money(110));
    REQUIRE(payment.capturedAt() == 12);
    REQUIRE(payment.refundIds().empty());
    REQUIRE(payment.refundedTotal(book.refunds()) == Money{});
    REQUIRE(book.findPaymentForOrder("order") == payment);
    REQUIRE_FALSE(book.findPaymentForOrder("missing"));
    REQUIRE(book.reconcileOrderBalance(order).empty());
    REQUIRE_FALSE(book.reconcileOrderBalance(test::paidOrder()).empty());
    const auto analysis = book.attemptOutcomeAnalysis(TimeWindow(11, 13));
    REQUIRE(analysis.orders == 1);
    REQUIRE(analysis.paidOrders == 1);
    REQUIRE(analysis.ordersWithDeclines == 1);
    REQUIRE(analysis.recoveredOrders == 1);
    REQUIRE(analysis.methods.at(PaymentMethod::SimulatedCard).declined == 1);
    REQUIRE(analysis.methods.at(PaymentMethod::SimulatedTransfer).succeeded == 1);
    REQUIRE(analysis.methods.at(PaymentMethod::SimulatedTransfer).attempts == 1);
    REQUIRE(book.attemptOutcomeAnalysis(TimeWindow(12, 13)).orders == 0);
    REQUIRE(book.attemptOutcomeAnalysis(TimeWindow(11, 12)).paidOrders == 0);
}

TEST_CASE("Refunds conserve captured money and retain exact return-document identity") {
    auto order = test::order();
    PaymentBook book;
    book.recordAttempt("a", "p", order, Money(110), PaymentMethod::SimulatedCard, 11, true, "",
                       "pay");
    const auto document = book.refund("r1", "refund1", "p", "return1", Money(34), 12);
    REQUIRE(document.id() == "r1");
    REQUIRE(document.operationId() == "refund1");
    REQUIRE(document.paymentId() == "p");
    REQUIRE(document.orderId() == "order");
    REQUIRE(document.returnRequestId() == "return1");
    REQUIRE(document.amount() == Money(34));
    REQUIRE(document.createdAt() == 12);
    REQUIRE(book.refund("ignored", "refund1", "p", "return1", Money(34), 15) == document);
    const auto before = book;
    REQUIRE_THROWS_AS(book.refund("r2", "refund2", "p", "return2", Money(77), 13),
                      PaymentRuleException);
    REQUIRE_THROWS_AS(book.refund("r2", "refund2", "p", "return2", Money(1), 10),
                      PaymentRuleException);
    REQUIRE_THROWS_AS(book.refund("r2", "refund2", "p", "return1", Money(1), 13),
                      DuplicateEntityException);
    REQUIRE_THROWS_AS(book.refund("r2", "refund1", "p", "return1", Money(33), 13),
                      DuplicateOperationException);
    REQUIRE_THROWS_AS(book.refund("r2", "pay", "p", std::nullopt, Money(1), 13),
                      DuplicateOperationException);
    REQUIRE(book == before);
    book.refund("r2", "refund2", "p", "return2", Money(76), 13);
    REQUIRE(book.refundedForOrder("order") == Money(110));
    REQUIRE(book.refundedForOrder("missing") == Money{});
    REQUIRE(book.payment("p").refundIds().size() == 2);
    REQUIRE(book.refunds().size() == 2);
    REQUIRE_THROWS_AS(book.refund("r3", "refund3", "p", "return3", Money(1), 14),
                      PaymentRuleException);
    REQUIRE_THROWS_AS(Refund("r", "op", "p", "o", std::nullopt, Money{}, 0),
                      InvalidDomainDataException);
}

TEST_CASE("Financial reconciliation distinguishes unpaid cancelled and partially returned orders") {
    auto order = test::order();
    PaymentBook book;
    REQUIRE(book.reconcileOrderBalance(order).empty());
    REQUIRE_FALSE(book.reconcileOrderBalance(test::paidOrder()).empty());
    book.recordAttempt("a", "p", order, Money(110), PaymentMethod::SimulatedCard, 11, true, "",
                       "pay");
    REQUIRE_FALSE(book.reconcileOrderBalance(order).empty());
    order.confirmPayment(11, "pay", "p");
    order.cancel(12, "cancel");
    REQUIRE_FALSE(book.reconcileOrderBalance(order).empty());
    book.refund("r", "refund", "p", std::nullopt, Money(110), 12);
    REQUIRE(book.reconcileOrderBalance(order).empty());
    REQUIRE_FALSE(book.reconcileOrderBalance(test::paidOrder()).empty());
    auto delivered = test::order();
    delivered.confirmPayment(11, "pay", "p");
    delivered.beginFulfillment(12, "pick", "f");
    delivered.markShipped(13, "ship", "f");
    delivered.markDelivered(14, "deliver", "f");
    REQUIRE(book.reconcileOrderBalance(delivered).empty());
}
