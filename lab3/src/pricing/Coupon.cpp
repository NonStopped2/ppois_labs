#include "shop/pricing/Coupon.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/PromotionRuleException.hpp"
#include "shop/common/Validation.hpp"

namespace shop {

std::string Coupon::normalizeCode(std::string_view code) {
    auto normalized = detail::text(code, "код купона");
    detail::require(normalized.size() <= 64, "Слишком длинный код купона");
    for (auto& ch : normalized) {
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 'a' + 'A');
        }
        detail::require((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-',
                        "Код купона содержит только латинские буквы, цифры и дефис");
    }
    return normalized;
}
Coupon::Coupon(std::string_view code, std::string_view promotionId, std::int64_t totalUseLimit,
               std::int64_t perCustomerLimit, TimeWindow window, bool enabled)
    : code_(normalizeCode(code)), promotionId_(detail::text(promotionId, "акция купона")),
      totalUseLimit_(totalUseLimit), perCustomerLimit_(perCustomerLimit), window_(window),
      enabled_(enabled) {
    detail::require(totalUseLimit > 0 && perCustomerLimit > 0 && perCustomerLimit <= totalUseLimit,
                    "Некорректные лимиты купона");
}
const std::string& Coupon::code() const noexcept {
    return code_;
}
const std::string& Coupon::promotionId() const noexcept {
    return promotionId_;
}
std::int64_t Coupon::totalUseLimit() const noexcept {
    return totalUseLimit_;
}
std::int64_t Coupon::perCustomerLimit() const noexcept {
    return perCustomerLimit_;
}
const TimeWindow& Coupon::window() const noexcept {
    return window_;
}
bool Coupon::enabled() const noexcept {
    return enabled_;
}
void Coupon::assertRedeemable(const Customer& customer, std::int64_t totalUsed,
                              std::int64_t customerUsed, Timestamp now) const {
    customer.assertMayCheckout();
    detail::check<PromotionRuleException>(enabled_ && window_.contains(now) && totalUsed >= 0 &&
                                              customerUsed >= 0 && customerUsed <= totalUsed &&
                                              totalUsed < totalUseLimit_ &&
                                              customerUsed < perCustomerLimit_,
                                          "Купон отключён, истёк или исчерпан");
}

} // namespace shop
