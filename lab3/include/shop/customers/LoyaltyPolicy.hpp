#pragma once

#include "shop/customers/LoyaltyAccount.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace shop {

/** Tier thresholds and discount ceilings are explicit store configuration. */
class LoyaltyPolicy {
  public:
    explicit LoyaltyPolicy(std::vector<Money> thresholds = {Money{}, Money(100000), Money(500000)},
                           std::vector<std::int64_t> discounts = {0, 300, 500},
                           std::int64_t maximumDiscountBps = 1000);
    std::size_t determineTier(const LoyaltyAccount& account) const;
    Money calculateDiscount(const LoyaltyAccount& account, Money subtotal) const;
    std::optional<Money> nextTierProgress(const LoyaltyAccount& account) const;
    const std::vector<Money>& thresholds() const noexcept;
    const std::vector<std::int64_t>& discounts() const noexcept;
    std::int64_t maximumDiscountBps() const noexcept;
    bool operator==(const LoyaltyPolicy&) const = default;

  private:
    std::vector<Money> tierThresholds_;
    std::vector<std::int64_t> tierDiscountBps_;
    std::int64_t maximumDiscountBps_;
};

} // namespace shop
