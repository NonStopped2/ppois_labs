#pragma once
#include "post/payments/Payment.hpp"

namespace post {

// Возврат относится к конкретной оплате, а не к компенсации за утрату отправления.
class Refund {
public:
    Refund(const std::string& id, const std::string& paymentId, const std::string& invoiceId,
           Money amount, std::chrono::sys_seconds createdAt, const std::string& reason);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& paymentId() const noexcept;
    [[nodiscard]] const std::string& invoiceId() const noexcept;
    [[nodiscard]] const Money& amount() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds createdAt() const noexcept;
    [[nodiscard]] const std::string& reason() const noexcept;
    void validateAgainst(const Payment& payment, Money alreadyRefunded) const;
    bool operator==(const Refund& other) const = default;

private:
    std::string id_;
    std::string paymentId_;
    std::string invoiceId_;
    Money amount_;
    std::chrono::sys_seconds createdAt_;
    std::string reason_;
};

} // namespace post
