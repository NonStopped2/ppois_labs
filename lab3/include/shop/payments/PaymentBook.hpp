#pragma once

#include "shop/orders/Order.hpp"
#include "shop/payments/Payment.hpp"
#include <optional>
#include <variant>
#include <vector>

namespace shop {
struct AttemptOutcome {
    std::string attemptId;
    std::optional<std::string> paymentId;
    bool successful = false;
    bool operator==(const AttemptOutcome&) const = default;
};
struct AttemptOperation {
    std::string orderId;
    Money amount;
    PaymentMethod method = PaymentMethod::SimulatedCard;
    bool accepted = false;
    std::string failureReason;
    AttemptOutcome result;
    bool operator==(const AttemptOperation&) const = default;
};
struct RefundOperation {
    std::string paymentId;
    std::optional<std::string> returnRequestId;
    Money amount;
    std::string resultId;
    bool operator==(const RefundOperation&) const = default;
};
using PaymentOperation = std::variant<AttemptOperation, RefundOperation>;
struct MethodOutcomes {
    std::int64_t attempts = 0;
    std::int64_t succeeded = 0;
    std::int64_t declined = 0;
    bool operator==(const MethodOutcomes&) const = default;
};
struct AttemptAnalysis {
    std::int64_t orders = 0;
    std::int64_t paidOrders = 0;
    std::int64_t ordersWithDeclines = 0;
    std::int64_t recoveredOrders = 0;
    std::map<PaymentMethod, MethodOutcomes> methods;
};

/** PaymentBook.
 * Ведёт попытки, платежи, возвраты и индекс операций. Сверяет повтор с исходными
 * параметрами до нового финансового эффекта.
 */
class PaymentBook {
  public:
    AttemptOutcome recordAttempt(std::string_view attemptId, std::string_view paymentId,
                                 const Order& order, Money amount, PaymentMethod method,
                                 Timestamp now, bool accepted, std::string_view failureReason,
                                 std::string_view operationId);
    Refund refund(std::string_view id, std::string_view operationId, std::string_view paymentId,
                  std::optional<std::string> returnRequestId, Money amount, Timestamp now);
    std::optional<Payment> findPaymentForOrder(std::string_view orderId) const;
    Payment payment(std::string_view id) const;
    Money refundedForOrder(std::string_view orderId) const;
    std::vector<std::string> reconcileOrderBalance(const Order& order) const;
    AttemptAnalysis attemptOutcomeAnalysis(const TimeWindow& period) const;
    const std::map<std::string, PaymentAttempt>& attempts() const noexcept;
    const std::map<std::string, Payment>& payments() const noexcept;
    const std::map<std::string, Refund>& refunds() const noexcept;
    const std::map<std::string, PaymentOperation>& operationIndex() const noexcept;
    bool operator==(const PaymentBook&) const = default;

  private:
    friend class SnapshotCodec;
    void swap(PaymentBook& other) noexcept;
    std::map<std::string, PaymentAttempt> attempts_;
    std::map<std::string, Payment> payments_;
    std::map<std::string, Refund> refunds_;
    std::map<std::string, PaymentOperation> operationIndex_;
};
} // namespace shop
