#pragma once

#include "shop/payments/PaymentAttempt.hpp"
#include "shop/payments/Refund.hpp"
#include <map>
#include <set>

namespace shop {
class Payment {
  public:
    Payment(std::string_view id, const PaymentAttempt& attempt);
    const std::string& id() const noexcept;
    const std::string& attemptId() const noexcept;
    const std::string& orderId() const noexcept;
    Money capturedAmount() const noexcept;
    Timestamp capturedAt() const noexcept;
    const std::set<std::string>& refundIds() const noexcept;
    Money refundedTotal(const std::map<std::string, Refund>& refunds) const;
    void assertRefundWithinBalance(const std::map<std::string, Refund>& refunds,
                                   Money amount) const;
    bool operator==(const Payment&) const = default;

  private:
    friend class PaymentBook;
    std::string id_;
    std::string attemptId_;
    std::string orderId_;
    Money capturedAmount_;
    Timestamp capturedAt_;
    std::set<std::string> refundIds_;
};
} // namespace shop
