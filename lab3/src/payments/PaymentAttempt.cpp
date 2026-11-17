#include "shop/payments/PaymentAttempt.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/PaymentRuleException.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
PaymentAttempt::PaymentAttempt(std::string_view id, std::string_view orderId, Money amount,
                               PaymentMethod method, Timestamp createdAt)
    : id_(detail::text(id, "попытка оплаты")), orderId_(detail::text(orderId, "заказ оплаты")),
      amount_(amount), method_(method), createdAt_(createdAt) {
    detail::require(
        amount > Money{} && createdAt >= 0 &&
            (method == PaymentMethod::SimulatedCard || method == PaymentMethod::SimulatedTransfer),
        "Некорректная попытка оплаты");
}
const std::string& PaymentAttempt::id() const noexcept {
    return id_;
}
const std::string& PaymentAttempt::orderId() const noexcept {
    return orderId_;
}
Money PaymentAttempt::amount() const noexcept {
    return amount_;
}
PaymentMethod PaymentAttempt::method() const noexcept {
    return method_;
}
Timestamp PaymentAttempt::createdAt() const noexcept {
    return createdAt_;
}
AttemptStatus PaymentAttempt::status() const noexcept {
    return status_;
}
const std::string& PaymentAttempt::failureReason() const noexcept {
    return failureReason_;
}
void PaymentAttempt::succeed() {
    detail::check<PaymentRuleException>(status_ == AttemptStatus::Pending,
                                        "Попытка оплаты уже завершена");
    status_ = AttemptStatus::Succeeded;
}
void PaymentAttempt::decline(std::string_view reason) {
    auto explanation = detail::text(reason, "причина отказа оплаты");
    detail::check<PaymentRuleException>(status_ == AttemptStatus::Pending,
                                        "Попытка оплаты уже завершена");
    failureReason_.swap(explanation);
    status_ = AttemptStatus::Declined;
}
} // namespace shop
