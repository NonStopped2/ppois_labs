#include "shop/payments/Refund.hpp"
#include "shop/common/Validation.hpp"
namespace shop {
Refund::Refund(std::string_view id, std::string_view operationId, std::string_view paymentId,
               std::string_view orderId, std::optional<std::string> returnRequestId, Money amount,
               Timestamp createdAt)
    : id_(detail::text(id, "возврат денег")),
      operationId_(detail::text(operationId, "операция возврата денег")),
      paymentId_(detail::text(paymentId, "платёж")), orderId_(detail::text(orderId, "заказ")),
      returnRequestId_(std::move(returnRequestId)), amount_(amount), createdAt_(createdAt) {
    detail::require(amount > Money{} && createdAt >= 0, "Некорректный документ возврата денег");
    if (returnRequestId_) {
        *returnRequestId_ = detail::text(*returnRequestId_, "заявка возврата");
    }
}
const std::string& Refund::id() const noexcept {
    return id_;
}
const std::string& Refund::operationId() const noexcept {
    return operationId_;
}
const std::string& Refund::paymentId() const noexcept {
    return paymentId_;
}
const std::string& Refund::orderId() const noexcept {
    return orderId_;
}
const std::optional<std::string>& Refund::returnRequestId() const noexcept {
    return returnRequestId_;
}
Money Refund::amount() const noexcept {
    return amount_;
}
Timestamp Refund::createdAt() const noexcept {
    return createdAt_;
}
} // namespace shop
