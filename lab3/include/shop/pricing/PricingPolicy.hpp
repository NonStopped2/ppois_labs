#pragma once
#include "shop/cart/Cart.hpp"
#include "shop/customers/LoyaltyPolicy.hpp"
#include "shop/delivery/DeliveryPolicy.hpp"
#include "shop/pricing/CouponBook.hpp"
#include "shop/pricing/PriceList.hpp"
#include "shop/pricing/PriceQuote.hpp"
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace shop {
struct PricingContext {
    PricingContext(const ProductCatalog& catalogValue, const PriceList& priceValues,
                   const std::map<std::string, Promotion>& promotionValues,
                   const CouponBook& couponValues, const LoyaltyAccount& accountValue,
                   const LoyaltyPolicy& loyaltyValue, const DeliveryPolicy& deliveryValue,
                   const ReturnPolicy& returnValue)
        : catalog(catalogValue), prices(priceValues), promotions(promotionValues),
          coupons(couponValues), loyaltyAccount(accountValue), loyaltyPolicy(loyaltyValue),
          deliveryPolicy(deliveryValue), returnPolicy(returnValue) {}
    const ProductCatalog& catalog;
    const PriceList& prices;
    const std::map<std::string, Promotion>& promotions;
    const CouponBook& coupons;
    const LoyaltyAccount& loyaltyAccount;
    const LoyaltyPolicy& loyaltyPolicy;
    const DeliveryPolicy& deliveryPolicy;
    const ReturnPolicy& returnPolicy;
};

/** PricingPolicy.
 * Рассчитывает предложение по корзине, ценам, скидкам и доставке. Расчёт не удерживает
 * товар или квоту купона.
 */
class PricingPolicy {
  public:
    explicit PricingPolicy(std::int64_t quoteLifetime = 300,
                           std::int64_t maximumCartUnits = DefaultMaximumCartUnits,
                           bool preferLoyaltyOnTie = true);
    std::int64_t quoteLifetime() const noexcept;
    std::int64_t maximumCartUnits() const noexcept;
    bool preferLoyaltyOnTie() const noexcept;
    static std::vector<Money> allocateDiscount(const std::vector<QuoteLine>& lines,
                                               const std::vector<std::size_t>& eligible,
                                               Money discount);
    PriceQuote selectDiscount(const std::vector<PriceQuote>& candidates) const;
    PriceQuote makeQuote(const Cart& cart, const Customer& customer, const Address& destination,
                         std::string_view deliveryOptionId, const PricingContext& context,
                         Timestamp now) const;
    bool operator==(const PricingPolicy&) const = default;

  private:
    std::int64_t quoteLifetime_;
    std::int64_t maximumCartUnits_;
    bool preferLoyaltyOnTie_;
};
} // namespace shop
