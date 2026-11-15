#include "ReportFacts.hpp"
#include "shop/reporting/SalesReport.hpp"
#include <algorithm>
#include <numeric>

namespace shop {
namespace {
void reconcileOrder(const Order& order, Timestamp end, const ReportContext& context,
                    std::vector<std::string>& issues) {
    const auto historical = order.stateBefore(end);
    Money captured;
    Money refunded;
    std::int64_t paymentCount = 0;
    for (const auto& [id, payment] : context.payments.payments()) {
        if (payment.orderId() != order.id() || payment.capturedAt() >= end) {
            continue;
        }
        ++paymentCount;
        captured = captured.checkedAdd(payment.capturedAmount());
    }
    for (const auto& [id, refund] : context.payments.refunds()) {
        if (refund.orderId() == order.id() && refund.createdAt() < end) {
            refunded = refunded.checkedAdd(refund.amount());
        }
    }
    const bool requiresPayment = historical.status == OrderStatus::Confirmed ||
                                 historical.status == OrderStatus::InFulfillment ||
                                 historical.status == OrderStatus::Shipped ||
                                 historical.status == OrderStatus::Delivered;
    const auto expected = order.goodsNet().checkedAdd(historical.delivery.fee);
    if ((requiresPayment && paymentCount != 1) || paymentCount > 1 ||
        (paymentCount && captured != expected)) {
        issues.push_back("Неверная оплата заказа: " + order.id());
    }
    if ((historical.status == OrderStatus::AwaitingPayment ||
         historical.status == OrderStatus::Expired) &&
        paymentCount) {
        issues.push_back("Оплата в неоплаченном состоянии: " + order.id());
    }
    if (refunded > captured ||
        (historical.status == OrderStatus::Cancelled && captured != refunded)) {
        issues.push_back("Неверный баланс возвратов: " + order.id());
    }
    if (requiresPayment && historical.status != OrderStatus::Delivered && refunded > Money{}) {
        issues.push_back("Возврат денег без отмены/получения: " + order.id());
    }
    report_detail::returnedBefore(order, end, context);
    for (const auto& event : order.history()) {
        if (event.kind() != OrderEventKind::ReturnCompleted || event.timestamp() >= end) {
            continue;
        }
        const auto& request = detail::get(context.requests, event.relatedEntityId());
        const auto& inspection = detail::get(context.inspections, *request.inspectionId());
        const auto amount = std::accumulate(
            request.lines().begin(), request.lines().end(), Money{},
            [&order, &inspection](Money sum, const auto& line) {
                return sum.checkedAdd(
                    order.line(line.sku())
                        .allocation()
                        .netForUnitRange(
                            line.alreadyReturnedOffset(),
                            detail::get(inspection.results(), line.sku()).acceptedQuantity()));
            });
        if (amount == Money{}) {
            if (request.refundId()) {
                issues.push_back("Лишний денежный документ нулевого возврата: " + request.id());
            }
        } else {
            detail::check<ReportRuleException>(request.refundId().has_value(),
                                               "Нет денежного документа возврата: " + request.id());
            const auto& refund = detail::get(context.payments.refunds(), *request.refundId());
            if (refund.amount() != amount || refund.createdAt() != event.timestamp() ||
                refund.returnRequestId() != request.id()) {
                issues.push_back("Деньги не совпадают с завершённым возвратом: " + request.id());
            }
        }
    }
}
void reconcilePayment(const Payment& payment, Timestamp end, const ReportContext& context,
                      std::vector<std::string>& issues) {
    const auto order = context.orders.order(payment.orderId());
    const auto& attempt = detail::get(context.payments.attempts(), payment.attemptId());
    if (attempt.status() != AttemptStatus::Succeeded || attempt.orderId() != order->id() ||
        attempt.amount() != payment.capturedAmount() ||
        attempt.createdAt() != payment.capturedAt()) {
        issues.push_back("Оплата не совпадает с попыткой: " + payment.id());
    }
    const bool event = std::any_of(
        order->history().begin(), order->history().end(), [&payment, end](const auto& entry) {
            return entry.kind() == OrderEventKind::PaymentConfirmed &&
                   entry.relatedEntityId() == payment.id() &&
                   entry.timestamp() == payment.capturedAt() && entry.timestamp() < end;
        });
    if (!event) {
        issues.push_back("Нет события подтверждения оплаты: " + payment.id());
    }
}
} // namespace
std::vector<std::string> SalesReport::reconcile(const ReportContext& context) const {
    report_detail::validatePeriod(period_, context);
    std::vector<std::string> issues;
    for (const auto& [id, order] : context.orders.orders()) {
        if (order->createdAt() >= period_.endsAt()) {
            continue;
        }
        try {
            reconcileOrder(*order, period_.endsAt(), context, issues);
        } catch (const ShopException& error) {
            issues.push_back("Заказ " + id + ": " + error.what());
        }
    }
    for (const auto& [id, payment] : context.payments.payments()) {
        if (payment.capturedAt() >= period_.endsAt()) {
            continue;
        }
        try {
            reconcilePayment(payment, period_.endsAt(), context, issues);
        } catch (const ShopException& error) {
            issues.push_back("Платёж " + id + ": " + error.what());
        }
    }
    for (const auto& [id, refund] : context.payments.refunds()) {
        if (refund.createdAt() >= period_.endsAt()) {
            continue;
        }
        try {
            const auto payment = context.payments.payment(refund.paymentId());
            const auto order = context.orders.order(refund.orderId());
            if (payment.orderId() != refund.orderId() ||
                payment.capturedAt() > refund.createdAt() || !payment.refundIds().contains(id)) {
                issues.push_back("Неверная связь возврата денег: " + id);
            }
            const bool event = std::any_of(
                order->history().begin(), order->history().end(), [&refund](const auto& entry) {
                    return entry.timestamp() == refund.createdAt() &&
                           (refund.returnRequestId()
                                ? entry.kind() == OrderEventKind::ReturnCompleted &&
                                      entry.relatedEntityId() == *refund.returnRequestId()
                                : entry.kind() == OrderEventKind::Cancelled);
                });
            if (!event) {
                issues.push_back("Возврат денег без события: " + id);
            }
        } catch (const ShopException& error) {
            issues.push_back("Возврат денег " + id + ": " + error.what());
        }
    }
    return issues;
}
} // namespace shop
