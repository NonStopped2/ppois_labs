#include "post/payments/Refund.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/PaymentException.hpp"

// Refund — возврат денег по конкретному Payment: хранит ссылки на платёж и счёт, сумму и причину.
// Проверяет время и невозвращённый остаток исходной оплаты; проводится через Invoice/CashRegister.
// Это возврат оплаты пересылки, а не компенсация за утрату или повреждение отправления.
namespace post {
// Непустое основание и положительная сумма обязательны до поиска исходной оплаты.
Refund::Refund(const std::string& id, const std::string& paymentId, const std::string& invoiceId,
               Money amount, std::chrono::sys_seconds createdAt, const std::string& reason)
    : id_(detail::identifier(id)), paymentId_(detail::identifier(paymentId)),
      invoiceId_(detail::identifier(invoiceId)), amount_(amount), createdAt_(createdAt),
      reason_(detail::normalizeText(reason)) {
    detail::requireTime(createdAt_);
    if (amount_ == Money{} || !detail::hasPrintableText(reason_)) {
        throw PaymentException("Refund requires amount and reason");
    }
}
// Идентификатор защищает от повторного проведения того же возврата.
const std::string& Refund::id() const noexcept {
    return id_;
}
// Возврат нельзя произвольно перенести на другую оплату.
const std::string& Refund::paymentId() const noexcept {
    return paymentId_;
}
// Идентификатор счёта должен совпадать и у возврата, и у оплаты.
const std::string& Refund::invoiceId() const noexcept {
    return invoiceId_;
}
// Сумма ограничивается ещё не возвращённой частью оплаты.
const Money& Refund::amount() const noexcept {
    return amount_;
}
// Возврат не может предшествовать исходной оплате.
std::chrono::sys_seconds Refund::createdAt() const noexcept {
    return createdAt_;
}
// Причина нормализована, но не заменяет отдельное решение о компенсации.
const std::string& Refund::reason() const noexcept {
    return reason_;
}
// Сначала проверяет вычитаемую сумму, затем безопасно вычисляет доступный остаток.
void Refund::validateAgainst(const Payment& payment, Money alreadyRefunded) const {
    if (paymentId_ != payment.id() || invoiceId_ != payment.invoiceId() ||
        createdAt_ < payment.paidAt() || alreadyRefunded > payment.amount() ||
        amount_ > payment.amount().subtract(alreadyRefunded)) {
        throw PaymentException("Refund does not fit original payment or remaining amount");
    }
}

} // namespace post
