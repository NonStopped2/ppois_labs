#include "shop/pricing/PriceEntry.hpp"
#include "shop/common/Validation.hpp"

namespace shop {

PriceEntry::PriceEntry(std::string_view sku, Money unitPrice, Timestamp changedAt,
                       std::int64_t revision)
    : sku_(detail::text(sku, "SKU")), unitPrice_(unitPrice), changedAt_(changedAt),
      revision_(revision) {
    detail::require(unitPrice > Money{} && changedAt >= 0 && revision > 0,
                    "Некорректная запись цены");
}
const std::string& PriceEntry::sku() const noexcept {
    return sku_;
}
Money PriceEntry::unitPrice() const noexcept {
    return unitPrice_;
}
Timestamp PriceEntry::changedAt() const noexcept {
    return changedAt_;
}
std::int64_t PriceEntry::revision() const noexcept {
    return revision_;
}

} // namespace shop
