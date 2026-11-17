#pragma once

#include "shop/catalog/ProductCatalog.hpp"
#include "shop/pricing/PriceEntry.hpp"
#include <map>
#include <string>
#include <string_view>

namespace shop {

/** PriceList.
 * Владеет текущими ценами. Массовое изменение по категории фиксируется целиком после
 * проверки всех цен.
 */
class PriceList {
  public:
    explicit PriceList(Money minimumUnitPrice = Money(1));
    void changePrice(const ProductCatalog& catalog, std::string_view sku, Money price,
                     Timestamp now);
    void applyCategoryPriceChange(const ProductCatalog& catalog, std::string_view categoryId,
                                  std::int64_t changeBps, Timestamp now);
    void removePriceForArchivedVariant(const ProductCatalog& catalog, std::string_view sku);
    PriceEntry price(std::string_view sku) const;
    const std::map<std::string, PriceEntry>& entries() const noexcept;
    std::int64_t revision() const noexcept;
    Money minimumUnitPrice() const noexcept;
    bool operator==(const PriceList&) const = default;

  private:
    friend class SnapshotCodec;
    std::map<std::string, PriceEntry> entries_;
    std::int64_t revision_ = 0;
    Money minimumUnitPrice_;
};

} // namespace shop
