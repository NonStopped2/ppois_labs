#include "shop/payments/PaymentBook.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/DuplicateOperationException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/PaymentRuleException.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {
void PaymentBook::swap(PaymentBook& other) noexcept {
    attempts_.swap(other.attempts_);
    payments_.swap(other.payments_);
    refunds_.swap(other.refunds_);
    operationIndex_.swap(other.operationIndex_);
}
AttemptOutcome PaymentBook::recordAttempt(std::string_view attemptId, std::string_view paymentId,
                                          const Order& order, Money amount, PaymentMethod method,
                                          Timestamp now, bool accepted,
                                          std::string_view failureReason,
                                          std::string_view operationId) {
    const auto key = detail::text(operationId, "операция оплаты");
    const auto reason = detail::text(failureReason, "причина отказа", accepted);
    detail::require(!accepted || reason.empty(), "У успешной оплаты не бывает причины отказа");
    const auto replay = operationIndex_.find(key);
    if (replay != operationIndex_.end()) {
        const auto* previous = std::get_if<AttemptOperation>(&replay->second);
        detail::check<DuplicateOperationException>(
            previous && previous->orderId == order.id() && previous->amount == amount &&
                previous->method == method && previous->accepted == accepted &&
                previous->failureReason == reason,
            "ID оплаты повторён с другими параметрами");
        return previous->result;
    }
    order.assertAwaitingPayment(now);
    for (const auto& [id, previous] : attempts_) {
        detail::check<PaymentRuleException>(previous.orderId() != order.id() ||
                                                previous.createdAt() <= now,
                                            "Попытка оплаты раньше предыдущей");
    }
    detail::check<PaymentRuleException>(amount == order.total() && !findPaymentForOrder(order.id()),
                                        "Сумма не совпадает с заказом или заказ уже оплачен");
    PaymentAttempt attempt(attemptId, order.id(), amount, method, now);
    if (accepted) {
        attempt.succeed();
    } else {
        attempt.decline(reason);
    }
    auto candidate = *this;
    detail::check<DuplicateEntityException>(
        candidate.attempts_.emplace(attempt.id(), attempt).second, "Повторный ID попытки");
    AttemptOutcome result{attempt.id(), std::nullopt, accepted};
    if (accepted) {
        const Payment document(paymentId, attempt);
        detail::check<DuplicateEntityException>(
            candidate.payments_.emplace(document.id(), document).second, "Повторный ID платежа");
        result.paymentId = document.id();
    }
    candidate.operationIndex_.emplace(
        key, AttemptOperation{order.id(), amount, method, accepted, reason, result});
    swap(candidate);
    return result;
}
Refund PaymentBook::refund(std::string_view id, std::string_view operationId,
                           std::string_view paymentId, std::optional<std::string> returnRequestId,
                           Money amount, Timestamp now) {
    const auto key = detail::text(operationId, "операция возврата денег");
    const auto paymentKey = detail::text(paymentId, "платёж");
    if (returnRequestId) {
        *returnRequestId = detail::text(*returnRequestId, "заявка возврата");
    }
    const auto replay = operationIndex_.find(key);
    if (replay != operationIndex_.end()) {
        const auto* previous = std::get_if<RefundOperation>(&replay->second);
        detail::check<DuplicateOperationException>(
            previous && previous->paymentId == paymentKey &&
                previous->returnRequestId == returnRequestId && previous->amount == amount,
            "ID возврата денег повторён с другими параметрами");
        return detail::get(refunds_, previous->resultId);
    }
    auto document = payment(paymentKey);
    document.assertRefundWithinBalance(refunds_, amount);
    detail::check<PaymentRuleException>(now >= document.capturedAt(), "Возврат раньше оплаты");
    for (const auto& refundKey : document.refundIds()) {
        const auto& previous = detail::get(refunds_, refundKey);
        detail::check<PaymentRuleException>(now >= previous.createdAt(),
                                            "Нарушена хронология возвратов денег");
        detail::check<DuplicateEntityException>(previous.returnRequestId() != returnRequestId,
                                                "По документу уже выполнен возврат денег");
    }
    Refund result(id, key, paymentKey, document.orderId(), returnRequestId, amount, now);
    auto candidate = *this;
    detail::check<DuplicateEntityException>(candidate.refunds_.emplace(result.id(), result).second,
                                            "Повторный ID возврата денег");
    document.refundIds_.insert(result.id());
    candidate.payments_.at(paymentKey) = std::move(document);
    candidate.operationIndex_.emplace(
        key, RefundOperation{paymentKey, returnRequestId, amount, result.id()});
    swap(candidate);
    return result;
}
std::optional<Payment> PaymentBook::findPaymentForOrder(std::string_view orderId) const {
    const auto found =
        std::find_if(payments_.begin(), payments_.end(),
                     [orderId](const auto& pair) { return pair.second.orderId() == orderId; });
    return found == payments_.end() ? std::nullopt : std::optional<Payment>(found->second);
}
Payment PaymentBook::payment(std::string_view id) const {
    return detail::get(payments_, id);
}
Money PaymentBook::refundedForOrder(std::string_view orderId) const {
    const auto document = findPaymentForOrder(orderId);
    return document ? document->refundedTotal(refunds_) : Money{};
}
std::vector<std::string> PaymentBook::reconcileOrderBalance(const Order& order) const {
    std::vector<std::string> issues;
    const auto document = findPaymentForOrder(order.id());
    const bool requiresPayment =
        order.status() == OrderStatus::Confirmed || order.status() == OrderStatus::InFulfillment ||
        order.status() == OrderStatus::Shipped || order.status() == OrderStatus::Delivered;
    if (requiresPayment && !document) {
        issues.push_back("Отсутствует оплата: " + order.id());
    }
    if (!document) {
        return issues;
    }
    const auto captureEvent =
        std::find_if(order.history().begin(), order.history().end(), [](const auto& event) {
            return event.kind() == OrderEventKind::PaymentConfirmed;
        });
    if (captureEvent == order.history().end() ||
        captureEvent->relatedEntityId() != document->id() ||
        captureEvent->timestamp() != document->capturedAt()) {
        issues.push_back("Платёж не согласован с историей заказа: " + order.id());
    }
    if (document->capturedAmount() != order.total()) {
        issues.push_back("Оплата не совпадает с итогом заказа: " + order.id());
    }
    if (order.status() == OrderStatus::AwaitingPayment || order.status() == OrderStatus::Expired) {
        issues.push_back("Оплата при неоплаченном состоянии: " + order.id());
    }
    const auto refunded = document->refundedTotal(refunds_);
    if (order.status() == OrderStatus::Cancelled && refunded != document->capturedAmount()) {
        issues.push_back("Отменённый заказ возмещён не полностью: " + order.id());
    }
    if (requiresPayment && order.status() != OrderStatus::Delivered && refunded != Money{}) {
        issues.push_back("Возврат денег до получения без отмены: " + order.id());
    }
    return issues;
}
AttemptAnalysis PaymentBook::attemptOutcomeAnalysis(const TimeWindow& period) const {
    std::map<std::string, Timestamp> firstAttempts;
    for (const auto& [id, attempt] : attempts_) {
        const auto [found, inserted] =
            firstAttempts.emplace(attempt.orderId(), attempt.createdAt());
        if (!inserted) {
            found->second = std::min(found->second, attempt.createdAt());
        }
    }
    AttemptAnalysis result;
    for (const auto& [orderId, first] : firstAttempts) {
        if (!period.contains(first)) {
            continue;
        }
        ++result.orders;
        bool paid = false;
        bool declined = false;
        for (const auto& [id, attempt] : attempts_) {
            if (attempt.orderId() != orderId || attempt.createdAt() >= period.endsAt()) {
                continue;
            }
            auto& summary = result.methods[attempt.method()];
            ++summary.attempts;
            if (attempt.status() == AttemptStatus::Succeeded) {
                ++summary.succeeded;
                paid = true;
            } else if (attempt.status() == AttemptStatus::Declined) {
                ++summary.declined;
                declined = true;
            }
        }
        result.paidOrders += paid;
        result.ordersWithDeclines += declined;
        result.recoveredOrders += paid && declined;
    }
    return result;
}
const std::map<std::string, PaymentAttempt>& PaymentBook::attempts() const noexcept {
    return attempts_;
}
const std::map<std::string, Payment>& PaymentBook::payments() const noexcept {
    return payments_;
}
const std::map<std::string, Refund>& PaymentBook::refunds() const noexcept {
    return refunds_;
}
const std::map<std::string, PaymentOperation>& PaymentBook::operationIndex() const noexcept {
    return operationIndex_;
}
} // namespace shop
