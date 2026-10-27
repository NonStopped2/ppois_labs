#pragma once
#include "post/core/Money.hpp"
#include "post/payments/PaymentMethod.hpp"

#include <chrono>
#include <string>

namespace post {
class Invoice;

// Неизменяемый положительный платёж; наличный/карточный способ не обращается к банку.
class Payment {
public:
    Payment(const std::string& id, const std::string& invoiceId, Money amount, PaymentMethod method,
            std::chrono::sys_seconds paidAt);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& invoiceId() const noexcept;
    [[nodiscard]] const Money& amount() const noexcept;
    [[nodiscard]] PaymentMethod method() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds paidAt() const noexcept;
    void validateFor(const Invoice& invoice) const;
    bool operator==(const Payment& other) const = default;

private:
    std::string id_;
    std::string invoiceId_;
    Money amount_;
    PaymentMethod method_;
    std::chrono::sys_seconds paidAt_;
};

} // namespace post
