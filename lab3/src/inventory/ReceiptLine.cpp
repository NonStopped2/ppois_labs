#include "shop/inventory/ReceiptLine.hpp"
#include "shop/common/Validation.hpp"
namespace shop {
ReceiptLine::ReceiptLine(std::string_view sku, std::int64_t quantity, Money purchaseUnitCost,
                         std::string_view batchReference)
    : sku_(detail::text(sku, "SKU")), quantity_(quantity), purchaseUnitCost_(purchaseUnitCost),
      batchReference_(detail::text(batchReference, "партия")) {
    detail::require(quantity > 0 && purchaseUnitCost >= Money{}, "Некорректная строка поставки");
    total();
}
const std::string& ReceiptLine::sku() const noexcept {
    return sku_;
}
std::int64_t ReceiptLine::quantity() const noexcept {
    return quantity_;
}
Money ReceiptLine::purchaseUnitCost() const noexcept {
    return purchaseUnitCost_;
}
const std::string& ReceiptLine::batchReference() const noexcept {
    return batchReference_;
}
Money ReceiptLine::total() const {
    return purchaseUnitCost_.multiply(quantity_);
}
} // namespace shop
