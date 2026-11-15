#include "shop/pricing/QuoteLine.hpp"
#include "shop/common/Validation.hpp"

namespace shop {

QuoteLine::QuoteLine(const ProductVariant& variant, std::string_view title, std::int64_t quantity,
                     Money unitPrice, Money discount, bool returnEligible)
    : sku_(variant.sku()), title_(detail::text(title, "название позиции")), quantity_(quantity),
      unitPrice_(unitPrice), allocation_(sku_, quantity, unitPrice.multiply(quantity), discount),
      netTotal_(allocation_.netTotal()),
      packageSnapshot_{variant.weightGrams(), variant.dimensionsMm(), variant.attributes()},
      returnEligible_(returnEligible) {
    detail::require(unitPrice > Money{}, "Цена единицы должна быть положительной");
}
const std::string& QuoteLine::sku() const noexcept {
    return sku_;
}
const std::string& QuoteLine::title() const noexcept {
    return title_;
}
std::int64_t QuoteLine::quantity() const noexcept {
    return quantity_;
}
Money QuoteLine::unitPrice() const noexcept {
    return unitPrice_;
}
const DiscountAllocation& QuoteLine::allocation() const noexcept {
    return allocation_;
}
Money QuoteLine::netTotal() const noexcept {
    return netTotal_;
}
const PackageSnapshot& QuoteLine::packageSnapshot() const noexcept {
    return packageSnapshot_;
}
bool QuoteLine::returnEligible() const noexcept {
    return returnEligible_;
}

QuoteLine QuoteLine::withDiscount(Money amount) const {
    auto candidate = *this;
    candidate.allocation_ =
        DiscountAllocation(sku_, quantity_, unitPrice_.multiply(quantity_), amount);
    candidate.netTotal_ = candidate.allocation_.netTotal();
    return candidate;
}

} // namespace shop
