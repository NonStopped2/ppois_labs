#include "shop/payments/Payment.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/PaymentRuleException.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
Payment::Payment(std::string_view id, const PaymentAttempt& attempt)
    : id_(detail::text(id, "платёж")), attemptId_(attempt.id()), orderId_(attempt.orderId()),
      capturedAmount_(attempt.amount()), capturedAt_(attempt.createdAt()) {
    detail::check<PaymentRuleException>(attempt.status() == AttemptStatus::Succeeded,
                                        "Нет успешной попытки оплаты");
}
const std::string& Payment::id() const noexcept {
    return id_;
}
const std::string& Payment::attemptId() const noexcept {
    return attemptId_;
}
const std::string& Payment::orderId() const noexcept {
    return orderId_;
}
Money Payment::capturedAmount() const noexcept {
    return capturedAmount_;
}
Timestamp Payment::capturedAt() const noexcept {
    return capturedAt_;
}
const std::set<std::string>& Payment::refundIds() const noexcept {
    return refundIds_;
}
Money Payment::refundedTotal(const std::map<std::string, Refund>& documents) const {
    Money total;
    for (const auto& key : refundIds_) {
        const auto& document = detail::get(documents, key);
        detail::check<PaymentRuleException>(document.paymentId() == id_ &&
                                                document.orderId() == orderId_ &&
                                                document.createdAt() >= capturedAt_,
                                            "Неверная связь документа возврата денег");
        total = total.checkedAdd(document.amount());
    }
    detail::check<PaymentRuleException>(total <= capturedAmount_,
                                        "Сумма возвратов превышает оплату");
    return total;
}
void Payment::assertRefundWithinBalance(const std::map<std::string, Refund>& documents,
                                        Money amount) const {
    detail::check<PaymentRuleException>(
        amount > Money{} && amount <= capturedAmount_.checkedSubtract(refundedTotal(documents)),
        "Возврат превышает остаток оплаты");
}
} // namespace shop
