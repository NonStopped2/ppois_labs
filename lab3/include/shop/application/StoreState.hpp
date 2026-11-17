#pragma once

#include "shop/application/OperationRecord.hpp"
#include "shop/catalog/ReviewBook.hpp"
#include "shop/common/ManualClock.hpp"
#include "shop/customers/AddressBook.hpp"
#include "shop/delivery/Fulfillment.hpp"
#include "shop/orders/OrderBook.hpp"
#include "shop/pricing/PricingPolicy.hpp"
#include "shop/reporting/SalesReport.hpp"
#include "shop/returns/ReturnInspection.hpp"
#include "shop/returns/ReturnRequest.hpp"

namespace shop {
/** StoreState.
 * Владеет реестрами магазина. Публикует согласованные изменения и выдаёт снимки для чтения.
 */
class StoreState {
  public:
    StoreState(Inventory inventory, DeliveryPolicy delivery, Timestamp initialTime = 0,
               PricingPolicy pricing = PricingPolicy{}, LoyaltyPolicy loyalty = LoyaltyPolicy{},
               ReturnPolicy returns = ReturnPolicy{}, std::int64_t paymentLifetime = 900,
               std::int64_t paymentExtension = 900);
    Timestamp now() const noexcept;
    void advanceClock(std::int64_t seconds);
    std::string nextId(std::string_view prefix);
    void addCustomer(const Customer& customer);
    void replaceCustomer(const Customer& customer);
    void replaceCatalog(const ProductCatalog& catalog);
    void changePrice(std::string_view sku, Money price);
    void applyCategoryPriceChange(std::string_view categoryId, std::int64_t changeBps);
    void applyStockCount(std::string_view sku, std::int64_t actual);
    void addPromotion(const Promotion& promotion);
    void registerCoupon(const Coupon& coupon);
    void disableCoupon(std::string_view code);
    void postReceipt(const StockReceipt& receipt);
    void replaceCart(const Cart& cart);
    void replaceAddresses(const AddressBook& book);
    void replaceWishlist(const Wishlist& wishlist);
    Customer customer(std::string_view id) const;
    Cart cart(std::string_view customerId) const;
    AddressBook addresses(std::string_view customerId) const;
    Wishlist wishlist(std::string_view customerId) const;
    LoyaltyAccount loyaltyAccount(std::string_view customerId) const;
    const ProductCatalog& catalog() const noexcept;
    const PriceList& prices() const noexcept;
    const std::map<std::string, Promotion>& promotions() const noexcept;
    const CouponBook& coupons() const noexcept;
    const Inventory& inventory() const noexcept;
    const OrderBook& orders() const noexcept;
    const PaymentBook& payments() const noexcept;
    const std::map<std::string, Fulfillment>& fulfillments() const noexcept;
    const OperationJournal& operations() const noexcept;
    const std::map<std::string, ReturnRequest>& returnRequests() const noexcept;
    const std::map<std::string, ReturnInspection>& inspections() const noexcept;
    const ReviewBook& reviews() const noexcept;
    std::string addReview(std::string_view actor, std::string_view orderId,
                          std::string_view productId, int rating, std::string_view text);
    void editReview(std::string_view actor, std::string_view reviewId, int rating,
                    std::string_view text);
    void moderateReview(std::string_view reviewId, bool publish, std::string_view reason = {});
    void voteHelpful(std::string_view actor, std::string_view reviewId);
    const PricingPolicy& pricingPolicy() const noexcept;
    const DeliveryPolicy& deliveryPolicy() const noexcept;
    PricingContext pricingContext(std::string_view customerId) const;
    ReportContext reportContext() const;
    bool operator==(const StoreState&) const = default;

  private:
    friend class PreparedChange;
    friend class Checkout;
    friend class OrderProcessing;
    friend class ReturnProcessing;
    friend class SnapshotCodec;
    static std::string allocateId(std::int64_t& sequence, std::string_view prefix);
    ProductCatalog catalog_;
    std::map<std::string, Customer> customers_;
    std::map<std::string, AddressBook> addressBooks_;
    std::map<std::string, Wishlist> wishlists_;
    std::map<std::string, LoyaltyAccount> loyaltyAccounts_;
    std::map<std::string, Cart> carts_;
    PriceList prices_;
    std::map<std::string, Promotion> promotions_;
    CouponBook coupons_;
    Inventory inventory_;
    OrderBook orders_;
    PaymentBook payments_;
    std::map<std::string, Fulfillment> fulfillments_;
    std::map<std::string, ReturnRequest> returnRequests_;
    std::map<std::string, ReturnInspection> inspections_;
    ReviewBook reviews_;
    OperationJournal operations_;
    ManualClock clock_;
    PricingPolicy pricingPolicy_;
    LoyaltyPolicy loyaltyPolicy_;
    DeliveryPolicy deliveryPolicy_;
    ReturnPolicy returnPolicy_;
    std::int64_t paymentLifetime_;
    std::int64_t paymentExtension_;
    std::int64_t nextSequence_ = 1;
};
} // namespace shop
