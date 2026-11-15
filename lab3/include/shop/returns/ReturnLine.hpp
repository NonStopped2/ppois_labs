#pragma once

#include "shop/orders/OrderLine.hpp"
#include "shop/returns/ReturnPolicy.hpp"

namespace shop {
struct ReturnSelection {
    std::string sku;
    std::int64_t quantity = 0;
    ReturnReason reason = ReturnReason::ChangedMind;
    bool operator==(const ReturnSelection&) const = default;
};
class ReturnLine {
  public:
    ReturnLine(const OrderLine& purchased, std::int64_t requestedQuantity,
               std::int64_t alreadyReturnedOffset, ReturnReason reason);
    const std::string& sku() const noexcept;
    std::int64_t requestedQuantity() const noexcept;
    std::int64_t alreadyReturnedOffset() const noexcept;
    ReturnReason reason() const noexcept;
    Money maximumRefund() const noexcept;
    bool operator==(const ReturnLine&) const = default;

  private:
    std::string sku_;
    std::int64_t requestedQuantity_;
    std::int64_t alreadyReturnedOffset_;
    ReturnReason reason_;
    Money maximumRefund_;
};
} // namespace shop
