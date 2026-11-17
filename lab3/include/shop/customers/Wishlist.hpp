#pragma once

#include "shop/catalog/ProductCatalog.hpp"
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace shop {

/** Wishlist.
 * Хранит идентификаторы понравившихся продуктов. Для переноса в корзину покупатель явно
 * выбирает вариант.
 */
class Wishlist {
  public:
    explicit Wishlist(std::string_view customerId);
    const std::string& customerId() const noexcept;
    const std::set<std::string>& productIds() const noexcept;
    void addProduct(const Product& product);
    void removeProduct(std::string_view productId);
    std::size_t removeArchived(const ProductCatalog& catalog);
    std::vector<std::string>
    createCartSelection(const ProductCatalog& catalog,
                        const std::map<std::string, std::string>& choices = {}) const;
    bool operator==(const Wishlist&) const = default;

  private:
    friend class SnapshotCodec;
    std::string customerId_;
    std::set<std::string> productIds_;
};

} // namespace shop
