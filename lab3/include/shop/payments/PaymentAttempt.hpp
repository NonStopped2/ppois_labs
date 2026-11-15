#pragma once

#include "shop/common/Money.hpp"
#include "shop/common/TimeWindow.hpp"
#include <string>
#include <string_view>

namespace shop {
enum class PaymentMethod { SimulatedCard, SimulatedTransfer };
enum class AttemptStatus { Pending, Succeeded, Declined };
class PaymentAttempt {
  public:
    PaymentAttempt(std::string_view id, std::string_view orderId, Money amount,
                   PaymentMethod method, Timestamp createdAt);
    const std::string& id() const noexcept;
    const std::string& orderId() const noexcept;
    Money amount() const noexcept;
    PaymentMethod method() const noexcept;
    Timestamp createdAt() const noexcept;
    AttemptStatus status() const noexcept;
    const std::string& failureReason() const noexcept;
    void succeed();
    void decline(std::string_view reason);
    bool operator==(const PaymentAttempt&) const = default;

  private:
    std::string id_;
    std::string orderId_;
    Money amount_;
    PaymentMethod method_;
    Timestamp createdAt_;
    AttemptStatus status_ = AttemptStatus::Pending;
    std::string failureReason_;
};
} // namespace shop
