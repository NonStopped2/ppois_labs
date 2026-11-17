#include "shop/customers/LoyaltyPolicy.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {

LoyaltyPolicy::LoyaltyPolicy(std::vector<Money> thresholds, std::vector<std::int64_t> discounts,
                             std::int64_t maximumDiscountBps)
    : tierThresholds_(std::move(thresholds)), tierDiscountBps_(std::move(discounts)),
      maximumDiscountBps_(maximumDiscountBps) {
    detail::require(!tierThresholds_.empty() && tierThresholds_.front() == Money{} &&
                        tierThresholds_.size() == tierDiscountBps_.size() &&
                        maximumDiscountBps >= 0 && maximumDiscountBps <= 10000,
                    "Несогласованные уровни лояльности");
    detail::require(std::is_sorted(tierThresholds_.begin(), tierThresholds_.end()) &&
                        std::adjacent_find(tierThresholds_.begin(), tierThresholds_.end()) ==
                            tierThresholds_.end(),
                    "Пороги лояльности должны строго возрастать от нуля");
    detail::require(std::is_sorted(tierDiscountBps_.begin(), tierDiscountBps_.end()) &&
                        std::all_of(tierDiscountBps_.begin(), tierDiscountBps_.end(),
                                    [](auto rate) { return rate >= 0 && rate <= 10000; }),
                    "Скидки уровней должны не убывать и находиться в диапазоне 0..10000");
}
std::size_t LoyaltyPolicy::determineTier(const LoyaltyAccount& account) const {
    const auto found =
        std::upper_bound(tierThresholds_.begin(), tierThresholds_.end(), account.qualifyingSpend());
    return static_cast<std::size_t>(found - tierThresholds_.begin() - 1);
}
Money LoyaltyPolicy::calculateDiscount(const LoyaltyAccount& account, Money subtotal) const {
    return subtotal.percentageFloor(
        std::min(tierDiscountBps_[determineTier(account)], maximumDiscountBps_));
}
std::optional<Money> LoyaltyPolicy::nextTierProgress(const LoyaltyAccount& account) const {
    const auto tier = determineTier(account);
    if (tier + 1 == tierThresholds_.size()) {
        return std::nullopt;
    }
    return tierThresholds_[tier + 1].checkedSubtract(account.qualifyingSpend());
}
const std::vector<Money>& LoyaltyPolicy::thresholds() const noexcept {
    return tierThresholds_;
}
const std::vector<std::int64_t>& LoyaltyPolicy::discounts() const noexcept {
    return tierDiscountBps_;
}
std::int64_t LoyaltyPolicy::maximumDiscountBps() const noexcept {
    return maximumDiscountBps_;
}

} // namespace shop
