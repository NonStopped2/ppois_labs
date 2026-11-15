#include "shop/orders/OrderLine.hpp"

namespace shop {
OrderLine::OrderLine(const QuoteLine& line)
    : sku_(line.sku()), title_(line.title()), quantity_(line.quantity()),
      unitPrice_(line.unitPrice()), allocation_(line.allocation()), netTotal_(line.netTotal()),
      weightGrams_(line.packageSnapshot().weightGrams),
      attributesSnapshot_(line.packageSnapshot().attributes),
      dimensionsMm_(line.packageSnapshot().dimensionsMm), returnEligible_(line.returnEligible()) {}
const std::string& OrderLine::sku() const noexcept {
    return sku_;
}
const std::string& OrderLine::title() const noexcept {
    return title_;
}
std::int64_t OrderLine::quantity() const noexcept {
    return quantity_;
}
Money OrderLine::unitPrice() const noexcept {
    return unitPrice_;
}
const DiscountAllocation& OrderLine::allocation() const noexcept {
    return allocation_;
}
Money OrderLine::netTotal() const noexcept {
    return netTotal_;
}
std::int64_t OrderLine::weightGrams() const noexcept {
    return weightGrams_;
}
const std::map<std::string, std::string>& OrderLine::attributesSnapshot() const noexcept {
    return attributesSnapshot_;
}
const std::array<std::int64_t, 3>& OrderLine::dimensionsMm() const noexcept {
    return dimensionsMm_;
}
bool OrderLine::returnEligible() const noexcept {
    return returnEligible_;
}
QuoteLine OrderLine::asQuoteLine() const {
    const ProductVariant variant(sku_, title_, attributesSnapshot_, weightGrams_, dimensionsMm_);
    return QuoteLine(variant, title_, quantity_, unitPrice_, allocation_.discountTotal(),
                     returnEligible_);
}
} // namespace shop
