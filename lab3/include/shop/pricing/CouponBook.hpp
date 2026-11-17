#pragma once

#include "shop/pricing/Coupon.hpp"
#include "shop/pricing/Promotion.hpp"
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace shop {

struct CouponUse {
    std::string code;
    std::string customerId;
    Timestamp heldAt = 0;
    std::optional<Timestamp> confirmedAt;
    bool operator==(const CouponUse&) const = default;
};

struct CouponUsage {
    std::int64_t held = 0;
    std::int64_t confirmed = 0;
    std::int64_t customerHeld = 0;
    std::int64_t customerConfirmed = 0;
    std::int64_t available = 0;
    bool operator==(const CouponUsage&) const = default;
};

/** CouponBook.
 * Хранит коды и их использования. Временное удержание занимает квоту до подтверждения
 * оплаты или освобождения.
 */
class CouponBook {
  public:
    void registerCode(const Coupon& coupon, const Promotion& promotion);
    void reserveUse(std::string_view code, const Customer& customer, const Promotion& promotion,
                    std::string_view orderId, Timestamp now);
    void confirmUse(std::string_view orderId, Timestamp now);
    void releaseUse(std::string_view orderId);
    void disableCode(std::string_view code);
    CouponUsage usageSummary(std::string_view code, std::string_view customerId) const;
    Coupon coupon(std::string_view code) const;
    const std::map<std::string, Coupon>& coupons() const noexcept;
    const std::map<std::string, CouponUse>& heldUses() const noexcept;
    const std::map<std::string, CouponUse>& confirmedUses() const noexcept;
    bool operator==(const CouponBook&) const = default;

  private:
    friend class SnapshotCodec;
    std::map<std::string, Coupon> coupons_;
    std::map<std::string, CouponUse> heldUses_;
    std::map<std::string, CouponUse> confirmedUses_;
};

} // namespace shop
