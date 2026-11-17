#include "shop/pricing/DiscountAllocation.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {

DiscountAllocation::DiscountAllocation(std::string_view sku, std::int64_t quantity,
                                       Money grossTotal, Money discountTotal)
    : sku_(detail::text(sku, "SKU")), quantity_(quantity), grossTotal_(grossTotal),
      discountTotal_(discountTotal) {
    detail::require(quantity > 0 && discountTotal >= Money{} && grossTotal >= discountTotal,
                    "Некорректное распределение скидки");
}
const std::string& DiscountAllocation::sku() const noexcept {
    return sku_;
}
std::int64_t DiscountAllocation::quantity() const noexcept {
    return quantity_;
}
Money DiscountAllocation::grossTotal() const noexcept {
    return grossTotal_;
}
Money DiscountAllocation::discountTotal() const noexcept {
    return discountTotal_;
}
Money DiscountAllocation::netTotal() const {
    return grossTotal_.checkedSubtract(discountTotal_);
}
Money DiscountAllocation::netForUnitRange(std::int64_t offset, std::int64_t count) const {
    detail::require(offset >= 0 && count >= 0 && offset <= quantity_ && count <= quantity_ - offset,
                    "Возвратный диапазон выходит за количество позиции");
    const auto net = netTotal().kopecks();
    const auto base = net / quantity_;
    const auto extra = net % quantity_;
    const auto extras = std::min(offset + count, extra) - std::min(offset, extra);
    return Money(base).multiply(count).checkedAdd(Money(extras));
}

} // namespace shop
