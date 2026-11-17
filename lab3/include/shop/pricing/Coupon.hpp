#pragma once

#include "shop/common/TimeWindow.hpp"
#include "shop/customers/Customer.hpp"
#include <cstdint>
#include <string>
#include <string_view>

namespace shop {

/** Coupon.
 * Ограничивает использование кода сроком, общим лимитом и лимитом на покупателя.
 */
class Coupon {
  public:
    Coupon(std::string_view code, std::string_view promotionId, std::int64_t totalUseLimit,
           std::int64_t perCustomerLimit, TimeWindow window, bool enabled = true);
    static std::string normalizeCode(std::string_view code);
    const std::string& code() const noexcept;
    const std::string& promotionId() const noexcept;
    std::int64_t totalUseLimit() const noexcept;
    std::int64_t perCustomerLimit() const noexcept;
    const TimeWindow& window() const noexcept;
    bool enabled() const noexcept;
    void assertRedeemable(const Customer& customer, std::int64_t totalUsed,
                          std::int64_t customerUsed, Timestamp now) const;
    bool operator==(const Coupon&) const = default;

  private:
    friend class CouponBook;
    std::string code_;
    std::string promotionId_;
    std::int64_t totalUseLimit_;
    std::int64_t perCustomerLimit_;
    TimeWindow window_;
    bool enabled_;
};

} // namespace shop
