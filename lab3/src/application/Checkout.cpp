#include "shop/application/Checkout.hpp"
#include "shop/application/PreparedChange.hpp"
#include "shop/common/CartRuleException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/InsufficientStockException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/QuoteChangedException.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>
#include <set>

namespace shop {
Checkout::Checkout(StoreState& state) : state_(state) {}
PriceQuote Checkout::preview(std::string_view actor, const Address& destination,
                             std::string_view deliveryOptionId) const {
    const auto customer = state_.customer(actor);
    const auto cart = state_.cart(actor);
    customer.assertMayCheckout();
    cart.assertCheckoutReady(actor, state_.pricingPolicy_.maximumCartUnits());
    for (const auto& [sku, line] : cart.lines()) {
        detail::check<InsufficientStockException>(state_.inventory_.available(sku) >=
                                                      line.quantity(),
                                                  "Недостаточно доступного товара: " + sku);
    }
    return state_.pricingPolicy_.makeQuote(cart, customer, destination, deliveryOptionId,
                                           state_.pricingContext(actor), state_.now());
}
// Повтор операции проверяем до повторного расчёта и удержания товара. Новые изменения готовятся
// вместе.
std::string Checkout::placeOrder(std::string_view actor, const PriceQuote& acceptedQuote,
                                 std::string_view operationId) {
    const CommandSignature signature{
        CommandKind::PlaceOrder, std::string(actor), "", {}, {}, std::nullopt, acceptedQuote};
    if (const auto replay = replayOperation(state_.operations_, operationId, signature)) {
        return std::get<std::string>(*replay);
    }
    const auto now = state_.now();
    detail::check<QuoteChangedException>(acceptedQuote.customerId() == actor,
                                         "Предложение принадлежит другому покупателю");
    acceptedQuote.assertUsable(now);
    const auto recalculated =
        preview(actor, acceptedQuote.deliveryAddress(), acceptedQuote.deliveryOptionId());
    detail::check<QuoteChangedException>(acceptedQuote.sameCommercialTerms(recalculated),
                                         "Условия покупки изменились; подтвердите новый расчёт");
    PreparedChange change(state_);
    auto id = change.nextId("order");
    const auto reservationId = change.nextId("reservation");
    const auto deadline = detail::addNonnegative(now, state_.paymentLifetime_);
    const Order order(id, recalculated, reservationId, now, deadline, operationId);
    change.inventory().reserve(reservationId, id, order.quantities(), now, deadline);
    if (order.discountSource().couponCode) {
        change.coupons().reserveUse(*order.discountSource().couponCode, state_.customer(actor),
                                    detail::get(state_.promotions_, order.discountSource().id), id,
                                    now);
    }
    change.orders().add(order);
    change.carts().at(std::string(actor)).clear();
    recordOperation(change.operations(), operationId, signature, id);
    change.commit();
    return id;
}
// Истечение освобождает товар и квоту. Технический ключ выбирается без коллизии с командами
// пользователя.
std::vector<std::string> Checkout::expireUnpaidOrders() {
    const auto now = state_.now();
    auto overdue = state_.orders_.findOverdueUnpaid(now);
    for (const auto& id : overdue) {
        auto order = *state_.orders_.order(id);
        const auto operation = unusedOperationId(state_.operations_, "system-expire-" + id);
        const CommandSignature signature{
            CommandKind::ExpireOrder, "system", id, {}, {}, std::nullopt, std::nullopt};
        PreparedChange change(state_);
        order.expire(now, operation);
        change.inventory().expire(order.reservationId(), now);
        if (order.discountSource().couponCode) {
            change.coupons().releaseUse(id);
        }
        change.orders().replace(order);
        recordOperation(change.operations(), operation, signature);
        change.commit();
    }
    return overdue;
}
void Checkout::moveCartItemsToWishlist(std::string_view actor, const std::vector<std::string>& skus,
                                       std::string_view operationId) {
    auto selected = skus;
    std::sort(selected.begin(), selected.end());
    const CommandSignature signature{CommandKind::MoveToWishlist,
                                     std::string(actor),
                                     "",
                                     selected,
                                     {},
                                     std::nullopt,
                                     std::nullopt};
    if (replayOperation(state_.operations_, operationId, signature)) {
        return;
    }
    detail::check<CartRuleException>(!selected.empty(), "Не выбраны позиции для избранного");
    detail::check<DuplicateEntityException>(std::adjacent_find(selected.begin(), selected.end()) ==
                                                selected.end(),
                                            "Повторный SKU в переносе");
    auto cart = state_.cart(actor);
    auto wishlist = state_.wishlist(actor);
    for (const auto& sku : selected) {
        detail::get(cart.lines(), sku);
        const auto product = state_.catalog_.productForSku(sku);
        detail::check<CartRuleException>(product.status() == ProductStatus::Published,
                                         "Недоступный продукт нельзя перенести в избранное");
        if (!wishlist.productIds().contains(product.id())) {
            wishlist.addProduct(product);
        }
        cart.removeItem(sku);
    }
    PreparedChange change(state_);
    change.carts().at(std::string(actor)) = std::move(cart);
    change.wishlists().at(std::string(actor)) = std::move(wishlist);
    recordOperation(change.operations(), operationId, signature);
    change.commit();
}
} // namespace shop
