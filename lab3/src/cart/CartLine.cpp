#include "shop/cart/CartLine.hpp"
#include "shop/common/Validation.hpp"
namespace shop {
CartLine::CartLine(std::string_view sku, std::int64_t quantity, Timestamp addedAt)
    : sku_(detail::text(sku, "SKU")), quantity_(quantity), addedAt_(addedAt) {
    detail::require(quantity > 0 && addedAt >= 0, "Некорректная строка корзины");
}
const std::string& CartLine::sku() const noexcept {
    return sku_;
}
std::int64_t CartLine::quantity() const noexcept {
    return quantity_;
}
Timestamp CartLine::addedAt() const noexcept {
    return addedAt_;
}
} // namespace shop
