#include "shop/inventory/StockItem.hpp"
#include "shop/common/InsufficientStockException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/ReservationStateException.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
StockItem::StockItem(std::string_view sku, std::int64_t onHand, std::int64_t reserved,
                     std::int64_t reorderPoint)
    : sku_(detail::text(sku, "SKU")), onHand_(onHand), reserved_(reserved),
      reorderPoint_(reorderPoint) {
    detail::require(reserved >= 0 && onHand >= reserved && reorderPoint >= 0,
                    "Некорректный остаток");
}
const std::string& StockItem::sku() const noexcept {
    return sku_;
}
std::int64_t StockItem::onHand() const noexcept {
    return onHand_;
}
std::int64_t StockItem::reserved() const noexcept {
    return reserved_;
}
std::int64_t StockItem::reorderPoint() const noexcept {
    return reorderPoint_;
}
std::int64_t StockItem::available() const noexcept {
    return onHand_ - reserved_;
}
void StockItem::receive(std::int64_t quantity) {
    detail::require(quantity > 0, "Приход должен быть положительным");
    onHand_ = detail::addNonnegative(onHand_, quantity);
}
void StockItem::hold(std::int64_t quantity) {
    detail::require(quantity > 0, "Резервируемое количество должно быть положительным");
    detail::check<InsufficientStockException>(quantity <= available(),
                                              "Недостаточно свободного остатка: " + sku_);
    reserved_ += quantity;
}
void StockItem::release(std::int64_t quantity) {
    detail::check<ReservationStateException>(quantity > 0 && quantity <= reserved_,
                                             "Нельзя освободить отсутствующий резерв");
    reserved_ -= quantity;
}
void StockItem::consumeHeld(std::int64_t quantity) {
    detail::check<ReservationStateException>(quantity > 0 && quantity <= reserved_,
                                             "Нельзя списать отсутствующий резерв");
    reserved_ -= quantity;
    onHand_ -= quantity;
}
void StockItem::applyCount(std::int64_t actual) {
    detail::check<InsufficientStockException>(
        actual >= reserved_, "Пересчёт не может уничтожить зарезервированные единицы");
    onHand_ = actual;
}
} // namespace shop
