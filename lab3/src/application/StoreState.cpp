#include "shop/application/StoreState.hpp"
#include "shop/application/ReturnProcessing.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"
#include <type_traits>

namespace shop {
StoreState::StoreState(Inventory inventory, DeliveryPolicy delivery, Timestamp initialTime,
                       PricingPolicy pricing, LoyaltyPolicy loyalty, ReturnPolicy returns,
                       std::int64_t paymentLifetime, std::int64_t paymentExtension)
    : inventory_(std::move(inventory)), clock_(initialTime), pricingPolicy_(pricing),
      loyaltyPolicy_(std::move(loyalty)), deliveryPolicy_(std::move(delivery)),
      returnPolicy_(std::move(returns)), paymentLifetime_(paymentLifetime),
      paymentExtension_(paymentExtension) {
    detail::require(paymentLifetime > 0 && paymentExtension > 0,
                    "Некорректные сроки оплаты магазина");
}
Timestamp StoreState::now() const noexcept {
    return clock_.now();
}
void StoreState::advanceClock(std::int64_t seconds) {
    clock_.advance(seconds);
}
std::string StoreState::nextId(std::string_view prefix) {
    const auto next = detail::addNonnegative(nextSequence_, 1);
    auto result = detail::text(prefix, "префикс ID") + "-" + std::to_string(nextSequence_);
    nextSequence_ = next;
    return result;
}
void StoreState::addCustomer(const Customer& entry) {
    detail::require(entry.registeredAt() <= now(), "Покупатель зарегистрирован в будущем");
    detail::check<DuplicateEntityException>(!customers_.contains(entry.id()),
                                            "Повторный покупатель");
    auto customers = customers_;
    auto candidateAddresses = addressBooks_;
    auto wishes = wishlists_;
    auto accounts = loyaltyAccounts_;
    auto carts = carts_;
    customers.emplace(entry.id(), entry);
    candidateAddresses.emplace(entry.id(), AddressBook(entry.id()));
    wishes.emplace(entry.id(), Wishlist(entry.id()));
    accounts.emplace(entry.id(), LoyaltyAccount(entry.id()));
    carts.emplace(entry.id(), Cart("cart-" + entry.id(), entry.id()));
    customers_.swap(customers);
    addressBooks_.swap(candidateAddresses);
    wishlists_.swap(wishes);
    loyaltyAccounts_.swap(accounts);
    carts_.swap(carts);
}
void StoreState::replaceCustomer(const Customer& entry) {
    const auto& original = detail::get(customers_, entry.id());
    detail::require(entry.registeredAt() == original.registeredAt(),
                    "Дата регистрации неизменяема");
    auto candidate = customers_;
    candidate.at(entry.id()) = entry;
    customers_.swap(candidate);
}
void StoreState::replaceCatalog(const ProductCatalog& values) {
    // Catalog edits may archive entries, but cannot orphan existing operational references.
    for (const auto& [id, entry] : catalog_.products()) {
        values.product(id);
    }
    for (const auto& [id, entry] : catalog_.categories()) {
        values.category(id);
    }
    for (const auto& [sku, entry] : prices_.entries()) {
        values.variant(sku);
    }
    for (const auto& [sku, entry] : inventory_.items()) {
        values.variant(sku);
    }
    for (const auto& [id, entry] : catalog_.products()) {
        for (const auto& variant : entry.variants()) {
            detail::require(values.productForSku(variant.sku()).id() == id,
                            "SKU нельзя передать другому продукту");
        }
    }
    auto candidate = values;
    static_assert(std::is_nothrow_swappable_v<ProductCatalog>);
    using std::swap;
    swap(catalog_, candidate);
}
void StoreState::changePrice(std::string_view sku, Money amount) {
    prices_.changePrice(catalog_, sku, amount, now());
}
void StoreState::applyCategoryPriceChange(std::string_view categoryId, std::int64_t changeBps) {
    prices_.applyCategoryPriceChange(catalog_, categoryId, changeBps, now());
}
void StoreState::applyStockCount(std::string_view sku, std::int64_t actual) {
    inventory_.applyStockCount(sku, actual);
}
void StoreState::addPromotion(const Promotion& offer) {
    for (const auto& id : offer.categoryIds()) {
        catalog_.category(id);
    }
    detail::check<DuplicateEntityException>(promotions_.emplace(offer.id(), offer).second,
                                            "Повторный ID акции");
}
void StoreState::registerCoupon(const Coupon& entry) {
    coupons_.registerCode(entry, detail::get(promotions_, entry.promotionId()));
}
void StoreState::disableCoupon(std::string_view code) {
    coupons_.disableCode(code);
}
void StoreState::postReceipt(const StockReceipt& receipt) {
    inventory_.postReceipt(receipt, catalog_);
}
void StoreState::replaceCart(const Cart& value) {
    const auto& original = detail::get(carts_, value.customerId());
    detail::require(value.id() == original.id(), "Нельзя заменить идентичность корзины");
    for (const auto& [sku, line] : value.lines()) {
        catalog_.variant(sku);
        detail::require(line.addedAt() <= now(), "Позиция добавлена в будущем");
    }
    auto candidate = carts_;
    candidate.at(value.customerId()) = value;
    carts_.swap(candidate);
}
void StoreState::replaceAddresses(const AddressBook& value) {
    detail::get(addressBooks_, value.customerId());
    auto candidate = addressBooks_;
    candidate.at(value.customerId()) = value;
    addressBooks_.swap(candidate);
}
void StoreState::replaceWishlist(const Wishlist& value) {
    detail::get(wishlists_, value.customerId());
    for (const auto& id : value.productIds()) {
        catalog_.product(id);
    }
    auto candidate = wishlists_;
    candidate.at(value.customerId()) = value;
    wishlists_.swap(candidate);
}
Customer StoreState::customer(std::string_view id) const {
    return detail::get(customers_, id);
}
Cart StoreState::cart(std::string_view customerId) const {
    return detail::get(carts_, customerId);
}
AddressBook StoreState::addresses(std::string_view customerId) const {
    return detail::get(addressBooks_, customerId);
}
Wishlist StoreState::wishlist(std::string_view customerId) const {
    return detail::get(wishlists_, customerId);
}
LoyaltyAccount StoreState::loyaltyAccount(std::string_view customerId) const {
    return detail::get(loyaltyAccounts_, customerId);
}
const ProductCatalog& StoreState::catalog() const noexcept {
    return catalog_;
}
const PriceList& StoreState::prices() const noexcept {
    return prices_;
}
const std::map<std::string, Promotion>& StoreState::promotions() const noexcept {
    return promotions_;
}
const CouponBook& StoreState::coupons() const noexcept {
    return coupons_;
}
const Inventory& StoreState::inventory() const noexcept {
    return inventory_;
}
const OrderBook& StoreState::orders() const noexcept {
    return orders_;
}
const PaymentBook& StoreState::payments() const noexcept {
    return payments_;
}
const std::map<std::string, Fulfillment>& StoreState::fulfillments() const noexcept {
    return fulfillments_;
}
const OperationJournal& StoreState::operations() const noexcept {
    return operations_;
}
const std::map<std::string, ReturnRequest>& StoreState::returnRequests() const noexcept {
    return returnRequests_;
}
const std::map<std::string, ReturnInspection>& StoreState::inspections() const noexcept {
    return inspections_;
}
const PricingPolicy& StoreState::pricingPolicy() const noexcept {
    return pricingPolicy_;
}
const DeliveryPolicy& StoreState::deliveryPolicy() const noexcept {
    return deliveryPolicy_;
}
PricingContext StoreState::pricingContext(std::string_view customerId) const {
    return {catalog_,
            prices_,
            promotions_,
            coupons_,
            detail::get(loyaltyAccounts_, customerId),
            loyaltyPolicy_,
            deliveryPolicy_,
            returnPolicy_};
}
const ReviewBook& StoreState::reviews() const noexcept {
    return reviews_;
}
std::string StoreState::addReview(std::string_view actor, std::string_view orderId,
                                  std::string_view productId, int rating, std::string_view text) {
    auto id = nextId("review");
    reviews_.addVerifiedReview(id, customer(actor), *orders_.order(orderId),
                               catalog_.product(productId),
                               ReturnProcessing(*this).completedQuantities(orderId), rating, text);
    return id;
}
void StoreState::editReview(std::string_view actor, std::string_view reviewId, int rating,
                            std::string_view text) {
    customer(actor);
    reviews_.edit(reviewId, actor, rating, text);
}
void StoreState::moderateReview(std::string_view reviewId, bool publish, std::string_view reason) {
    reviews_.moderate(reviewId, publish, reason);
}
void StoreState::voteHelpful(std::string_view actor, std::string_view reviewId) {
    reviews_.voteHelpful(reviewId, customer(actor));
}
ReportContext StoreState::reportContext() const {
    return {orders_, payments_, returnRequests_, inspections_, customers_, now()};
}
} // namespace shop
