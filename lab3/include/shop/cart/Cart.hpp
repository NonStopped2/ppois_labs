#pragma once
#include "shop/cart/CartLine.hpp"
#include "shop/customers/Wishlist.hpp"
#include "shop/inventory/Inventory.hpp"
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace shop {
inline constexpr std::int64_t DefaultMaximumCartUnits = 1000;

/** Editable wishes; quantities in a cart do not hold inventory. */
class Cart {
  public:
    Cart(std::string_view id, std::string_view customerId);
    const std::string& id() const noexcept;
    const std::string& customerId() const noexcept;
    const std::map<std::string, CartLine>& lines() const noexcept;
    const std::optional<std::string>& couponCode() const noexcept;
    std::int64_t revision() const noexcept;
    std::int64_t totalUnits() const;
    void addItem(const ProductCatalog& catalog, std::string_view sku, std::int64_t quantity,
                 Timestamp now, std::int64_t maximumUnits = DefaultMaximumCartUnits);
    void changeQuantity(std::string_view sku, std::int64_t quantity,
                        std::int64_t maximumUnits = DefaultMaximumCartUnits);
    void removeItem(std::string_view sku);
    void setCoupon(std::optional<std::string> code);
    void mergeWishlist(const Wishlist& wishlist, const ProductCatalog& catalog,
                       const std::map<std::string, std::string>& choices, Timestamp now,
                       std::int64_t maximumUnits = DefaultMaximumCartUnits);
    std::size_t removeUnavailable(const ProductCatalog& catalog);
    void replaceVariant(const ProductCatalog& catalog, std::string_view oldSku,
                        std::string_view newSku);
    std::map<std::string, std::pair<std::int64_t, std::int64_t>>
    adjustToAvailableStock(const Inventory& inventory);
    void assertCheckoutReady(std::string_view actor, std::int64_t maximumUnits) const;
    void clear();
    bool operator==(const Cart&) const = default;

  private:
    friend class SnapshotCodec;
    void replaceLines(std::map<std::string, CartLine> candidate);
    std::string id_;
    std::string customerId_;
    std::map<std::string, CartLine> lines_;
    std::optional<std::string> couponCode_;
    std::int64_t revision_ = 0;
};
} // namespace shop
