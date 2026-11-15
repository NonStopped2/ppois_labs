#pragma once
#include "post/payments/InvoiceStatus.hpp"
#include "post/payments/Payment.hpp"
#include "post/payments/Refund.hpp"
#include "post/tariffs/PostageQuote.hpp"

namespace post {

// Invoice — счёт на пересылку: хранит PostageQuote и журналы Payment/Refund, считает долг.
// Проверяет актуальность расчёта и суммы; возврат денег снова увеличивает задолженность.
// CashRegister согласует изменения счёта и кассы; Refunded не означает отмену счёта.
class Invoice {
public:
    Invoice(const std::string& id, const PostageQuote& quote, std::chrono::sys_seconds issuedAt,
            std::chrono::sys_seconds expiresAt);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const PostageQuote& quote() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds issuedAt() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds expiresAt() const noexcept;
    [[nodiscard]] InvoiceStatus status() const noexcept;
    [[nodiscard]] const std::vector<Payment>& payments() const noexcept;
    [[nodiscard]] const std::vector<Refund>& refunds() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds latestActivityAt() const noexcept;
    [[nodiscard]] Money amountDue() const;
    void registerPayment(const Payment& payment, const Shipment& shipment);
    void registerRefund(const Refund& refund);
    void cancel();
    bool operator==(const Invoice& other) const = default;

private:
    [[nodiscard]] Money refundedFor(const Payment& payment) const;
    std::string id_;
    PostageQuote quote_;
    std::chrono::sys_seconds issuedAt_;
    std::chrono::sys_seconds expiresAt_;
    InvoiceStatus status_;
    std::vector<Payment> payments_;
    std::vector<Refund> refunds_;
};

} // namespace post
