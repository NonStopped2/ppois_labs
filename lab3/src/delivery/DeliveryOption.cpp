#include "shop/delivery/DeliveryOption.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {
DeliveryOption::DeliveryOption(std::string_view id, std::string_view name,
                               std::set<std::string> postalPrefixes,
                               std::int64_t maximumWeightGrams, std::int64_t maximumDimensionMm,
                               Money baseFee, Money freeFromSubtotal)
    : id_(detail::text(id, "доставка")), name_(detail::text(name, "название доставки")),
      postalPrefixes_(std::move(postalPrefixes)), maximumWeightGrams_(maximumWeightGrams),
      maximumDimensionMm_(maximumDimensionMm), baseFee_(baseFee),
      freeFromSubtotal_(freeFromSubtotal) {
    detail::require(maximumWeightGrams > 0 && maximumDimensionMm > 0 && baseFee >= Money{} &&
                        freeFromSubtotal >= Money{},
                    "Некорректные ограничения доставки");
    for (const auto& prefix : postalPrefixes_) {
        detail::require(prefix.size() <= 6 && detail::digits(prefix),
                        "Некорректный индексный префикс доставки");
    }
}
const std::string& DeliveryOption::id() const noexcept {
    return id_;
}
const std::string& DeliveryOption::name() const noexcept {
    return name_;
}
const std::set<std::string>& DeliveryOption::postalPrefixes() const noexcept {
    return postalPrefixes_;
}
std::int64_t DeliveryOption::maximumWeightGrams() const noexcept {
    return maximumWeightGrams_;
}
std::int64_t DeliveryOption::maximumDimensionMm() const noexcept {
    return maximumDimensionMm_;
}
Money DeliveryOption::baseFee() const noexcept {
    return baseFee_;
}
Money DeliveryOption::freeFromSubtotal() const noexcept {
    return freeFromSubtotal_;
}
bool DeliveryOption::accepts(const Address& destination,
                             const std::vector<QuoteLine>& lines) const {
    if (lines.empty()) {
        return false;
    }
    if (!postalPrefixes_.empty() &&
        std::none_of(postalPrefixes_.begin(), postalPrefixes_.end(),
                     [&destination](const auto& prefix) {
                         return destination.postalCode().starts_with(prefix);
                     })) {
        return false;
    }
    std::int64_t weight = 0;
    for (const auto& line : lines) {
        const auto lineWeight =
            Money(line.packageSnapshot().weightGrams).multiply(line.quantity()).kopecks();
        weight = detail::addNonnegative(weight, lineWeight);
        if (std::any_of(line.packageSnapshot().dimensionsMm.begin(),
                        line.packageSnapshot().dimensionsMm.end(),
                        [this](auto dimension) { return dimension > maximumDimensionMm_; })) {
            return false;
        }
    }
    return weight <= maximumWeightGrams_;
}
Money DeliveryOption::calculateFee(Money goodsNet) const {
    detail::require(goodsNet >= Money{}, "Отрицательная товарная сумма доставки");
    return goodsNet >= freeFromSubtotal_ ? Money{} : baseFee_;
}
} // namespace shop
