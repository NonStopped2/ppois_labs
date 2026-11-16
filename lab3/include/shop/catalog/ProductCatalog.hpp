#pragma once

#include "shop/catalog/Category.hpp"
#include "shop/catalog/Product.hpp"
#include "shop/common/Money.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace shop {
class PriceList;
class Inventory;
struct ComparisonColumn {
    std::string title;
    std::optional<std::pair<Money, Money>> priceRange;
    std::map<std::string, std::int64_t> availableBySku;
};
struct ComparisonAttribute {
    bool comparable = true;
    std::map<std::string, std::optional<AttributeKind>> kinds;
    std::map<std::string, std::set<std::string>> values;
};
struct ProductComparison {
    std::map<std::string, ComparisonColumn> products;
    std::map<std::string, ComparisonAttribute> attributes;
};

/** Owns the catalog and enforces tree integrity and global SKU uniqueness. */
class ProductCatalog {
  public:
    void addCategory(const Category& category);
    void addProduct(const Product& product);
    void addVariant(std::string_view productId, const ProductVariant& variant);
    void replaceAttributeSchema(std::string_view productId, std::vector<ProductAttribute> schema);
    void publish(std::string_view productId);
    void archiveProduct(std::string_view productId);
    void restoreProductDraft(std::string_view productId);
    void setVariantAvailability(std::string_view sku, bool enabled);
    void moveCategory(std::string_view categoryId, std::optional<std::string> parentId);
    void archiveCategory(std::string_view categoryId);
    void mergeCategories(std::string_view sourceId, std::string_view targetId);
    void bulkMoveProducts(const std::vector<std::string>& productIds, std::string_view categoryId);
    std::vector<Product> search(std::string_view query = {},
                                std::optional<std::string> categoryId = std::nullopt,
                                const std::map<std::string, std::string>& attributes = {}) const;
    std::vector<ProductVariant>
    findCompatibleVariants(std::string_view productId,
                           const std::map<std::string, std::string>& attributes) const;
    bool isInCategory(std::string_view categoryId, std::string_view ancestorId) const;
    ProductComparison compareProducts(const std::vector<std::string>& productIds,
                                      const PriceList& prices, const Inventory& inventory) const;
    Product product(std::string_view productId) const;
    Product productForSku(std::string_view sku) const;
    ProductVariant variant(std::string_view sku) const;
    Category category(std::string_view categoryId) const;
    std::int64_t revision() const noexcept;
    const std::map<std::string, Category>& categories() const noexcept;
    const std::map<std::string, Product>& products() const noexcept;
    bool operator==(const ProductCatalog&) const = default;

  private:
    friend class SnapshotCodec;
    void checkSiblingName(const Category& category,
                          const std::optional<std::string>& parentId) const;
    void replaceProduct(Product candidate);
    std::map<std::string, Category> categories_;
    std::map<std::string, Product> products_;
    std::map<std::string, std::string> skuIndex_;
    std::int64_t revision_ = 0;
};

} // namespace shop
