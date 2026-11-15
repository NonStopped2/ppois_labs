#include "shop/pricing/Promotion.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/PromotionRuleException.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>
#include <numeric>

namespace shop {

Promotion::Promotion(std::string_view id, std::string_view name, PromotionKind kind,
                     std::int64_t value, Money minimumSubtotal, TimeWindow window,
                     std::set<std::string> categoryIds, bool automatic, bool enabled)
    : id_(detail::text(id, "ID акции")), name_(detail::text(name, "акция")), kind_(kind),
      value_(value), minimumSubtotal_(minimumSubtotal), window_(window),
      categoryIds_(std::move(categoryIds)), automatic_(automatic), enabled_(enabled) {
    detail::require(value > 0 && minimumSubtotal >= Money{} &&
                        (kind == PromotionKind::FixedAmount ||
                         (kind == PromotionKind::Percentage && value <= 10000)),
                    "Некорректные параметры акции");
    for (const auto& idValue : categoryIds_) {
        detail::text(idValue, "категория акции");
    }
}
const std::string& Promotion::id() const noexcept {
    return id_;
}
const std::string& Promotion::name() const noexcept {
    return name_;
}
PromotionKind Promotion::kind() const noexcept {
    return kind_;
}
std::int64_t Promotion::value() const noexcept {
    return value_;
}
Money Promotion::minimumSubtotal() const noexcept {
    return minimumSubtotal_;
}
const TimeWindow& Promotion::window() const noexcept {
    return window_;
}
const std::set<std::string>& Promotion::categoryIds() const noexcept {
    return categoryIds_;
}
bool Promotion::automatic() const noexcept {
    return automatic_;
}
bool Promotion::enabled() const noexcept {
    return enabled_;
}

std::vector<std::size_t> Promotion::eligibleLines(const std::vector<QuoteLine>& lines,
                                                  const ProductCatalog& catalog,
                                                  Timestamp now) const {
    std::vector<std::size_t> result;
    if (!enabled_ || !window_.contains(now)) {
        return result;
    }
    for (const auto& idValue : categoryIds_) {
        catalog.category(idValue);
    }
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto item = catalog.productForSku(lines[index].sku());
        const auto matches = [this, &item, &catalog] {
            return categoryIds_.empty() ||
                   std::any_of(categoryIds_.begin(), categoryIds_.end(),
                               [&item, &catalog](const auto& ancestor) {
                                   return catalog.isInCategory(item.categoryId(), ancestor);
                               });
        };
        if (matches()) {
            result.push_back(index);
        }
    }
    return result;
}
Money Promotion::calculateDiscount(const std::vector<QuoteLine>& lines,
                                   const ProductCatalog& catalog, Timestamp now) const {
    const auto selected = eligibleLines(lines, catalog, now);
    const auto subtotal = std::accumulate(
        selected.begin(), selected.end(), Money{}, [&lines](Money sum, std::size_t index) {
            return sum.checkedAdd(lines[index].allocation().grossTotal());
        });
    if (selected.empty() || subtotal < minimumSubtotal_) {
        return Money{};
    }
    return kind_ == PromotionKind::Percentage ? subtotal.percentageFloor(value_)
                                              : std::min(subtotal, Money(value_));
}
void Promotion::assertApplicable(const std::vector<QuoteLine>& lines, const ProductCatalog& catalog,
                                 Timestamp now) const {
    detail::check<PromotionRuleException>(calculateDiscount(lines, catalog, now) > Money{},
                                          "Акция неприменима или не даёт скидку");
}

} // namespace shop
