#pragma once

#include "shop/catalog/ProductVariant.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace shop {

enum class ProductStatus { Draft, Published, Archived };

/** Product.
 * Объединяет схему характеристик и варианты. Публикация возможна только при наличии
 * доступного варианта.
 */
class Product {
  public:
    Product(std::string_view id, std::string_view categoryId, std::string_view title,
            std::string_view description, std::vector<ProductAttribute> attributes = {});
    const std::string& id() const noexcept;
    const std::string& categoryId() const noexcept;
    const std::string& title() const noexcept;
    const std::string& description() const noexcept;
    const std::vector<ProductAttribute>& attributes() const noexcept;
    const std::vector<ProductVariant>& variants() const noexcept;
    ProductStatus status() const noexcept;
    void addVariant(const ProductVariant& variant);
    void replaceAttributeSchema(std::vector<ProductAttribute> attributes);
    void publish();
    void archive();
    void restoreDraft();
    bool operator==(const Product&) const = default;

  private:
    friend class ProductCatalog;
    std::string id_;
    std::string categoryId_;
    std::string title_;
    std::string description_;
    std::vector<ProductAttribute> attributes_;
    std::vector<ProductVariant> variants_;
    ProductStatus status_ = ProductStatus::Draft;
};

} // namespace shop
