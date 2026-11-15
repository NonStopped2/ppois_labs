#include "post/payments/Invoice.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/PaymentException.hpp"

#include <algorithm>
#include <numeric>

// Invoice — счёт на пересылку: хранит PostageQuote и журналы Payment/Refund, считает долг.
// Проверяет актуальность расчёта и суммы; возврат денег снова увеличивает задолженность.
// CashRegister согласует изменения счёта и кассы; Refunded не означает отмену счёта.
namespace post {
// Срок счёта целиком находится внутри срока снимка расчёта; бесплатный счёт сразу оплачен.
Invoice::Invoice(const std::string& id, const PostageQuote& quote,
                 std::chrono::sys_seconds issuedAt, std::chrono::sys_seconds expiresAt)
    : id_(detail::identifier(id)), quote_(quote), issuedAt_(issuedAt), expiresAt_(expiresAt),
      status_(quote.total() == Money{} ? InvoiceStatus::Paid : InvoiceStatus::Open) {
    detail::requireTime(issuedAt_);
    detail::requireTime(expiresAt_);
    if (issuedAt_ < quote_.createdAt() || expiresAt_ > quote_.expiresAt() ||
        expiresAt_ < issuedAt_) {
        throw PaymentException("Invoice validity must be inside quote validity");
    }
}
// Идентификатор связывает счёт с платежами и возвратами.
const std::string& Invoice::id() const noexcept {
    return id_;
}
// Снимок расчёта не изменяется при оплате и возврате.
const PostageQuote& Invoice::quote() const noexcept {
    return quote_;
}
// Нижняя граница допустимой оплаты включена.
std::chrono::sys_seconds Invoice::issuedAt() const noexcept {
    return issuedAt_;
}
// Верхняя граница ограничивает оплату, но не запрещает поздний возврат.
std::chrono::sys_seconds Invoice::expiresAt() const noexcept {
    return expiresAt_;
}
// Статус согласован с зачтёнными средствами, а не с суммой всех исторических поступлений.
InvoiceStatus Invoice::status() const noexcept {
    return status_;
}
// Журнал оплат нельзя пополнить в обход проверок registerPayment.
const std::vector<Payment>& Invoice::payments() const noexcept {
    return payments_;
}
// Возвраты сохраняют связь с конкретными исходными оплатами.
const std::vector<Refund>& Invoice::refunds() const noexcept {
    return refunds_;
}
// Благодаря общей хронологии достаточно сравнить последние элементы двух журналов.
std::chrono::sys_seconds Invoice::latestActivityAt() const noexcept {
    const auto paid = payments_.empty() ? issuedAt_ : payments_.back().paidAt();
    const auto refunded = refunds_.empty() ? issuedAt_ : refunds_.back().createdAt();
    return std::max(paid, refunded);
}
// Сумма возвратов одной оплаты не превышает её величину по инварианту проведения.
Money Invoice::refundedFor(const Payment& payment) const {
    return std::accumulate(
        refunds_.begin(), refunds_.end(), Money{}, [&payment](Money sum, const auto& refund) {
            return refund.paymentId() == payment.id() ? sum.add(refund.amount()) : sum;
        });
}
// Сначала вычитает возвраты каждой оплаты: огромный исторический оборот не переполняет текущий
// баланс.
Money Invoice::amountDue() const {
    if (status_ == InvoiceStatus::Cancelled) {
        return Money{};
    }
    const auto credited = std::accumulate(
        payments_.begin(), payments_.end(), Money{}, [this](Money sum, const auto& payment) {
            return sum.add(payment.amount().subtract(refundedFor(payment)));
        });
    return quote_.total().subtract(credited);
}
// Все бизнес-проверки и вычисление нового статуса предшествуют добавлению в журнал.
void Invoice::registerPayment(const Payment& payment, const Shipment& shipment) {
    payment.validateFor(*this);
    quote_.validateFor(shipment, payment.paidAt());
    const auto remaining = amountDue().subtract(payment.amount());
    const auto next = remaining == Money{} ? InvoiceStatus::Paid
                                           : (refunds_.empty() ? InvoiceStatus::PartiallyPaid
                                                               : InvoiceStatus::PartiallyRefunded);
    payments_.push_back(payment);
    status_ = next;
}
// Возврат после истечения счёта разрешён, но не задним числом и не сверх остатка конкретной оплаты.
void Invoice::registerRefund(const Refund& refund) {
    if (status_ == InvoiceStatus::Cancelled || refund.createdAt() < latestActivityAt() ||
        std::any_of(refunds_.begin(), refunds_.end(),
                    [&refund](const auto& prior) { return prior.id() == refund.id(); })) {
        throw PaymentException("Refund duplicates an operation or violates invoice state/time");
    }
    const auto payment =
        std::find_if(payments_.begin(), payments_.end(),
                     [&refund](const auto& paid) { return paid.id() == refund.paymentId(); });
    if (payment == payments_.end()) {
        throw PaymentException("Refund refers to unknown payment");
    }
    refund.validateAgainst(*payment, refundedFor(*payment));
    const auto reopened = amountDue().add(refund.amount());
    const auto next =
        reopened == quote_.total() ? InvoiceStatus::Refunded : InvoiceStatus::PartiallyRefunded;
    refunds_.push_back(refund);
    status_ = next;
}
// Нельзя отменить счёт с удержанными средствами; после полного возврата отмена допустима.
void Invoice::cancel() {
    if (status_ == InvoiceStatus::Cancelled || amountDue() != quote_.total()) {
        throw PaymentException("Invoice still holds funds or is already cancelled");
    }
    status_ = InvoiceStatus::Cancelled;
}

} // namespace post
