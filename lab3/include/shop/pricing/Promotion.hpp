#pragma once

#include "shop/catalog/ProductCatalog.hpp"
#include "shop/common/TimeWindow.hpp"
#include "shop/pricing/QuoteLine.hpp"
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace shop {

enum class PromotionKind { Percentage, FixedAmount };

/** Promotion eligibility is evaluated before allocating discounts into purchase snapshots. */
class Promotion {
  public:
    Promotion(std::string_view id, std::string_view name, PromotionKind kind, std::int64_t value,
              Money minimumSubtotal, TimeWindow window, std::set<std::string> categoryIds = {},
              bool automatic = true, bool enabled = true);
    const std::string& id() const noexcept;
    const std::string& name() const noexcept;
    PromotionKind kind() const noexcept;
    std::int64_t value() const noexcept;
    Money minimumSubtotal() const noexcept;
    const TimeWindow& window() const noexcept;
    const std::set<std::string>& categoryIds() const noexcept;
    bool automatic() const noexcept;
    bool enabled() const noexcept;
    std::vector<std::size_t> eligibleLines(const std::vector<QuoteLine>& lines,
                                           const ProductCatalog& catalog, Timestamp now) const;
    Money calculateDiscount(const std::vector<QuoteLine>& lines, const ProductCatalog& catalog,
                            Timestamp now) const;
    void assertApplicable(const std::vector<QuoteLine>& lines, const ProductCatalog& catalog,
                          Timestamp now) const;
    bool operator==(const Promotion&) const = default;

  private:
    std::string id_;
    std::string name_;
    PromotionKind kind_;
    std::int64_t value_;
    Money minimumSubtotal_;
    TimeWindow window_;
    std::set<std::string> categoryIds_;
    bool automatic_;
    bool enabled_;
};

} // namespace shop
