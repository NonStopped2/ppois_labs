#include "shop/pricing/PriceQuote.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/QuoteChangedException.hpp"
#include "shop/common/Validation.hpp"
#include <numeric>
#include <set>

namespace shop {
PriceQuote::PriceQuote(std::string_view customerId, std::vector<QuoteLine> lines,
                       Address deliveryAddress, std::string_view deliveryOptionId,
                       Money deliveryFee, DiscountSource discountSource, Timestamp createdAt,
                       Timestamp validUntil, ReturnPolicy returnPolicy)
    : customerId_(detail::text(customerId, "покупатель предложения")), lines_(std::move(lines)),
      deliveryAddress_(std::move(deliveryAddress)),
      deliveryOptionId_(detail::text(deliveryOptionId, "доставка")), deliveryFee_(deliveryFee),
      discountSource_(std::move(discountSource)), createdAt_(createdAt), validUntil_(validUntil),
      returnPolicySnapshot_(std::move(returnPolicy)) {
    detail::require(!lines_.empty() && createdAt >= 0 && validUntil > createdAt &&
                        deliveryFee >= Money{},
                    "Некорректное предложение");
    std::set<std::string> seen;
    for (const auto& entry : lines_) {
        detail::check<DuplicateEntityException>(seen.insert(entry.sku()).second,
                                                "SKU повторён в предложении");
    }
    const auto discount =
        std::accumulate(lines_.begin(), lines_.end(), Money{}, [](Money sum, const auto& entry) {
            return sum.checkedAdd(entry.allocation().discountTotal());
        });
    const auto kind = discountSource_.kind;
    detail::require(kind == DiscountKind::None || kind == DiscountKind::Promotion ||
                        kind == DiscountKind::Coupon || kind == DiscountKind::Loyalty,
                    "Неизвестный источник скидки");
    detail::require((kind == DiscountKind::None) == (discount == Money{}),
                    "Источник скидки противоречит сумме");
    detail::require((kind == DiscountKind::Coupon) == discountSource_.couponCode.has_value(),
                    "Код купона противоречит источнику скидки");
    detail::require(kind == DiscountKind::None ? discountSource_.id.empty()
                                               : !discountSource_.id.empty(),
                    "Некорректный ID источника скидки");
    total_ = goodsNet().checkedAdd(deliveryFee_);
    detail::require(total_ > Money{}, "Нулевые заказы не поддерживаются");
}
const std::string& PriceQuote::customerId() const noexcept {
    return customerId_;
}
const std::vector<QuoteLine>& PriceQuote::lines() const noexcept {
    return lines_;
}
const Address& PriceQuote::deliveryAddress() const noexcept {
    return deliveryAddress_;
}
const std::string& PriceQuote::deliveryOptionId() const noexcept {
    return deliveryOptionId_;
}
Money PriceQuote::deliveryFee() const noexcept {
    return deliveryFee_;
}
const DiscountSource& PriceQuote::discountSource() const noexcept {
    return discountSource_;
}
Timestamp PriceQuote::createdAt() const noexcept {
    return createdAt_;
}
Timestamp PriceQuote::validUntil() const noexcept {
    return validUntil_;
}
Money PriceQuote::total() const noexcept {
    return total_;
}
const ReturnPolicy& PriceQuote::returnPolicy() const noexcept {
    return returnPolicySnapshot_;
}
Money PriceQuote::goodsNet() const {
    return std::accumulate(lines_.begin(), lines_.end(), Money{}, [](Money sum, const auto& entry) {
        return sum.checkedAdd(entry.netTotal());
    });
}
void PriceQuote::assertUsable(Timestamp now) const {
    detail::check<QuoteChangedException>(now >= createdAt_ && now < validUntil_,
                                         "Предложение истекло или создано в будущем");
}
bool PriceQuote::sameCommercialTerms(const PriceQuote& other) const {
    return customerId_ == other.customerId_ && lines_ == other.lines_ &&
           deliveryAddress_ == other.deliveryAddress_ &&
           deliveryOptionId_ == other.deliveryOptionId_ && deliveryFee_ == other.deliveryFee_ &&
           discountSource_ == other.discountSource_ && total_ == other.total_ &&
           returnPolicySnapshot_ == other.returnPolicySnapshot_;
}
} // namespace shop
