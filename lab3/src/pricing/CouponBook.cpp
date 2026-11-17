#include "shop/pricing/CouponBook.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/PromotionRuleException.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {

void CouponBook::registerCode(const Coupon& entry, const Promotion& promotion) {
    detail::check<PromotionRuleException>(entry.promotionId() == promotion.id(),
                                          "Купон относится к другой акции");
    detail::check<DuplicateEntityException>(coupons_.emplace(entry.code(), entry).second,
                                            "Код купона уже существует");
}

CouponUsage CouponBook::usageSummary(std::string_view code, std::string_view customerId) const {
    const auto entry = coupon(code);
    CouponUsage result;
    for (const auto& [id, use] : heldUses_) {
        if (use.code == entry.code()) {
            ++result.held;
            result.customerHeld += use.customerId == customerId;
        }
    }
    for (const auto& [id, use] : confirmedUses_) {
        if (use.code == entry.code()) {
            ++result.confirmed;
            result.customerConfirmed += use.customerId == customerId;
        }
    }
    result.available =
        std::min(entry.totalUseLimit() - result.held - result.confirmed,
                 entry.perCustomerLimit() - result.customerHeld - result.customerConfirmed);
    return result;
}

// Удержанная попытка уже занимает часть лимита, чтобы два заказа не использовали одну квоту.
void CouponBook::reserveUse(std::string_view code, const Customer& customer,
                            const Promotion& promotion, std::string_view orderId, Timestamp now) {
    const auto order = detail::text(orderId, "заказ");
    detail::check<DuplicateEntityException>(
        !heldUses_.contains(order) && !confirmedUses_.contains(order), "Заказ уже применил купон");
    const auto entry = coupon(code);
    detail::check<PromotionRuleException>(entry.promotionId() == promotion.id() &&
                                              promotion.enabled() &&
                                              promotion.window().contains(now),
                                          "Акция купона недоступна");
    const auto usage = usageSummary(entry.code(), customer.id());
    entry.assertRedeemable(customer, usage.held + usage.confirmed,
                           usage.customerHeld + usage.customerConfirmed, now);
    heldUses_.emplace(order, CouponUse{entry.code(), customer.id(), now, std::nullopt});
}

// Оплата подтверждает ранее удержанную квоту, поэтому срок акции повторно не проверяется.
void CouponBook::confirmUse(std::string_view orderId, Timestamp now) {
    const auto order = std::string(orderId);
    auto use = detail::get(heldUses_, orderId);
    detail::check<PromotionRuleException>(now >= use.heldAt, "Оплата купона раньше удержания");
    use.confirmedAt = now;
    auto confirmed = confirmedUses_;
    confirmed.emplace(order, use);
    confirmedUses_.swap(confirmed);
    heldUses_.erase(order);
}
void CouponBook::releaseUse(std::string_view orderId) {
    detail::check<PromotionRuleException>(!confirmedUses_.contains(std::string(orderId)),
                                          "Оплаченное применение не восстанавливается");
    detail::get(heldUses_, orderId);
    heldUses_.erase(std::string(orderId));
}
void CouponBook::disableCode(std::string_view code) {
    const auto key = Coupon::normalizeCode(code);
    detail::get(coupons_, key);
    coupons_.at(key).enabled_ = false;
}
Coupon CouponBook::coupon(std::string_view code) const {
    return detail::get(coupons_, Coupon::normalizeCode(code));
}
const std::map<std::string, Coupon>& CouponBook::coupons() const noexcept {
    return coupons_;
}
const std::map<std::string, CouponUse>& CouponBook::heldUses() const noexcept {
    return heldUses_;
}
const std::map<std::string, CouponUse>& CouponBook::confirmedUses() const noexcept {
    return confirmedUses_;
}

} // namespace shop
