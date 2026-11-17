#pragma once

#include "shop/catalog/ProductAttribute.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace shop {

/** ProductVariant.
 * Представляет продаваемый SKU с характеристиками и данными упаковки. По ним считаются
 * доступность и доставка.
 */
class ProductVariant {
  public:
    ProductVariant(std::string_view sku, std::string_view label,
                   std::map<std::string, std::string> attributes, std::int64_t weightGrams,
                   std::array<std::int64_t, 3> dimensionsMm, bool saleEnabled = true);
    const std::string& sku() const noexcept;
    const std::string& label() const noexcept;
    const std::map<std::string, std::string>& attributes() const noexcept;
    std::int64_t weightGrams() const noexcept;
    const std::array<std::int64_t, 3>& dimensionsMm() const noexcept;
    bool saleEnabled() const noexcept;
    bool matchesAttributes(const std::map<std::string, std::string>& selection) const;
    bool conformsTo(const std::vector<ProductAttribute>& schema) const;
    bool operator==(const ProductVariant&) const = default;

  private:
    friend class ProductCatalog;
    friend class Product;
    std::string sku_;
    std::string label_;
    std::map<std::string, std::string> attributes_;
    std::int64_t weightGrams_;
    std::array<std::int64_t, 3> dimensionsMm_;
    bool saleEnabled_;
};

} // namespace shop
