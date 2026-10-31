#include "post/payments/Payment.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/PaymentException.hpp"
#include "post/payments/Invoice.hpp"

#include <algorithm>

// Payment — запись одной положительной оплаты: ID, счёт, сумма, способ и время.
// Проверяет связь со счётом, срок, хронологию и отсутствие повторного ID платежа.
// Создание записи не проводит оплату: её регистрируют Invoice и CashRegister; банка здесь нет.
namespace post {
// Проверяет собственные данные независимо от того, существует ли указанный счёт.
Payment::Payment(const std::string& id, const std::string& invoiceId, Money amount,
                 PaymentMethod method, std::chrono::sys_seconds paidAt)
    : id_(detail::identifier(id)), invoiceId_(detail::identifier(invoiceId)), amount_(amount),
      method_(method), paidAt_(paidAt) {
    detail::requireTime(paidAt_);
    if (amount_ == Money{} || (method_ != PaymentMethod::Cash && method_ != PaymentMethod::Card)) {
        throw PaymentException("Payment needs a positive amount and a known method");
    }
}
// Идентификатор используется для защиты от повторного проведения.
const std::string& Payment::id() const noexcept {
    return id_;
}
// Связь с конкретным счётом проверяется перед проведением.
const std::string& Payment::invoiceId() const noexcept {
    return invoiceId_;
}
// Сумма является самостоятельным значением Money.
const Money& Payment::amount() const noexcept {
    return amount_;
}
// Способ оплаты понадобится кассе для отдельного учёта наличности.
PaymentMethod Payment::method() const noexcept {
    return method_;
}
// Дата задаётся вызывающим процессом, а не считывается из часов.
std::chrono::sys_seconds Payment::paidAt() const noexcept {
    return paidAt_;
}
// Проверяет финансовые ограничения; живое отправление дополнительно проверит Invoice.
void Payment::validateFor(const Invoice& invoice) const {
    if (invoiceId_ != invoice.id() || invoice.status() == InvoiceStatus::Cancelled ||
        paidAt_ < invoice.issuedAt() || paidAt_ > invoice.expiresAt() ||
        paidAt_ < invoice.latestActivityAt() || amount_ > invoice.amountDue()) {
        throw PaymentException("Payment does not fit invoice, balance or time");
    }
    if (std::any_of(invoice.payments().begin(), invoice.payments().end(),
                    [this](const auto& payment) { return payment.id() == id_; })) {
        throw PaymentException("Duplicate payment id");
    }
}

} // namespace post
