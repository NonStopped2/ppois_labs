#include "shop/cart/Cart.hpp"
#include "shop/common/CartRuleException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"
#include "shop/pricing/Coupon.hpp"
#include <algorithm>
#include <numeric>

namespace shop {
namespace {
void sellable(const ProductCatalog& catalog, std::string_view sku) {
    detail::check<CartRuleException>(catalog.productForSku(sku).status() ==
                                             ProductStatus::Published &&
                                         catalog.variant(sku).saleEnabled(),
                                     "Вариант недоступен для новой покупки");
}
} // namespace
Cart::Cart(std::string_view id, std::string_view customerId)
    : id_(detail::text(id, "корзина")), customerId_(detail::text(customerId, "покупатель")) {}
const std::string& Cart::id() const noexcept {
    return id_;
}
const std::string& Cart::customerId() const noexcept {
    return customerId_;
}
const std::map<std::string, CartLine>& Cart::lines() const noexcept {
    return lines_;
}
const std::optional<std::string>& Cart::couponCode() const noexcept {
    return couponCode_;
}
std::int64_t Cart::revision() const noexcept {
    return revision_;
}
std::int64_t Cart::totalUnits() const {
    return std::accumulate(lines_.begin(), lines_.end(), std::int64_t{0},
                           [](auto sum, const auto& pair) {
                               return detail::addNonnegative(sum, pair.second.quantity());
                           });
}
void Cart::replaceLines(std::map<std::string, CartLine> candidate) {
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    lines_.swap(candidate);
    revision_ = nextRevision;
}
void Cart::addItem(const ProductCatalog& catalog, std::string_view sku, std::int64_t quantity,
                   Timestamp now, std::int64_t maximumUnits) {
    sellable(catalog, sku);
    detail::check<CartRuleException>(quantity > 0 && maximumUnits > 0 &&
                                         detail::addNonnegative(totalUnits(), quantity) <=
                                             maximumUnits,
                                     "Превышен предел количества корзины");
    auto candidate = lines_;
    const auto found = candidate.find(std::string(sku));
    const auto previous = found == candidate.end() ? 0 : found->second.quantity();
    const auto added = found == candidate.end() ? now : found->second.addedAt();
    detail::require(now >= added, "Нельзя добавлять товар задним числом");
    CartLine entry(sku, detail::addNonnegative(previous, quantity), added);
    candidate.insert_or_assign(entry.sku(), entry);
    replaceLines(std::move(candidate));
}
void Cart::changeQuantity(std::string_view sku, std::int64_t quantity, std::int64_t maximumUnits) {
    const auto previous = detail::get(lines_, sku);
    detail::check<CartRuleException>(
        quantity > 0 && maximumUnits > 0 &&
            detail::addNonnegative(totalUnits() - previous.quantity(), quantity) <= maximumUnits,
        "Неверное новое количество корзины");
    auto candidate = lines_;
    candidate.at(std::string(sku)) = CartLine(sku, quantity, previous.addedAt());
    replaceLines(std::move(candidate));
}
void Cart::removeItem(std::string_view sku) {
    detail::get(lines_, sku);
    auto candidate = lines_;
    candidate.erase(std::string(sku));
    replaceLines(std::move(candidate));
}
void Cart::setCoupon(std::optional<std::string> code) {
    if (code) {
        *code = Coupon::normalizeCode(*code);
    }
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    couponCode_.swap(code);
    revision_ = nextRevision;
}
void Cart::mergeWishlist(const Wishlist& wishlist, const ProductCatalog& catalog,
                         const std::map<std::string, std::string>& choices, Timestamp now,
                         std::int64_t maximumUnits) {
    detail::check<CartRuleException>(wishlist.customerId() == customerId_, "Чужое избранное");
    const auto selection = wishlist.createCartSelection(catalog, choices);
    auto candidate = *this;
    for (const auto& sku : selection) {
        candidate.addItem(catalog, sku, 1, now, maximumUnits);
    }
    replaceLines(std::move(candidate.lines_));
}
std::size_t Cart::removeUnavailable(const ProductCatalog& catalog) {
    auto candidate = lines_;
    for (const auto& [sku, entry] : lines_) {
        if (catalog.productForSku(sku).status() != ProductStatus::Published ||
            !catalog.variant(sku).saleEnabled()) {
            candidate.erase(sku);
        }
    }
    const auto removed = lines_.size() - candidate.size();
    if (removed > 0) {
        replaceLines(std::move(candidate));
    }
    return removed;
}
void Cart::replaceVariant(const ProductCatalog& catalog, std::string_view oldSku,
                          std::string_view newSku) {
    const auto original = detail::get(lines_, oldSku);
    sellable(catalog, newSku);
    detail::check<CartRuleException>(oldSku != newSku && catalog.productForSku(oldSku).id() ==
                                                             catalog.productForSku(newSku).id(),
                                     "Замена разрешена только на другой вариант того же продукта");
    auto candidate = lines_;
    candidate.erase(std::string(oldSku));
    const auto existing = candidate.find(std::string(newSku));
    const auto amount =
        existing == candidate.end()
            ? original.quantity()
            : detail::addNonnegative(existing->second.quantity(), original.quantity());
    const auto added = existing == candidate.end()
                           ? original.addedAt()
                           : std::min(existing->second.addedAt(), original.addedAt());
    CartLine replacement(newSku, amount, added);
    candidate.insert_or_assign(replacement.sku(), replacement);
    replaceLines(std::move(candidate));
}
std::map<std::string, std::pair<std::int64_t, std::int64_t>>
Cart::adjustToAvailableStock(const Inventory& inventory) {
    auto candidate = lines_;
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> changes;
    for (const auto& [sku, entry] : lines_) {
        const auto adjusted = std::min(entry.quantity(), inventory.available(sku));
        if (adjusted == entry.quantity()) {
            continue;
        }
        changes.emplace(sku, std::make_pair(entry.quantity(), adjusted));
        if (adjusted == 0) {
            candidate.erase(sku);
        } else {
            candidate.at(sku) = CartLine(sku, adjusted, entry.addedAt());
        }
    }
    if (!changes.empty()) {
        replaceLines(std::move(candidate));
    }
    return changes;
}
void Cart::assertCheckoutReady(std::string_view actor, std::int64_t maximumUnits) const {
    detail::check<CartRuleException>(actor == customerId_ && !lines_.empty() && maximumUnits > 0 &&
                                         totalUnits() <= maximumUnits,
                                     "Пустая, чужая или слишком большая корзина");
}
void Cart::clear() {
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    lines_.clear();
    couponCode_.reset();
    revision_ = nextRevision;
}
} // namespace shop
