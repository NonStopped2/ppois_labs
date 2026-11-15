#include "shop/catalog/Product.hpp"
#include "shop/common/CatalogRuleException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>
#include <set>

namespace shop {
namespace {
void checkSchema(const std::vector<ProductAttribute>& attributes) {
    std::set<std::string> names;
    for (const auto& attribute : attributes) {
        detail::check<DuplicateEntityException>(names.insert(attribute.name()).second,
                                                "Повторная характеристика продукта");
    }
}
} // namespace

Product::Product(std::string_view id, std::string_view categoryId, std::string_view title,
                 std::string_view description, std::vector<ProductAttribute> attributes)
    : id_(detail::text(id, "ID продукта")), categoryId_(detail::text(categoryId, "категория")),
      title_(detail::text(title, "название продукта")),
      description_(detail::text(description, "описание", true)),
      attributes_(std::move(attributes)) {
    checkSchema(attributes_);
}
const std::string& Product::id() const noexcept {
    return id_;
}
const std::string& Product::categoryId() const noexcept {
    return categoryId_;
}
const std::string& Product::title() const noexcept {
    return title_;
}
const std::string& Product::description() const noexcept {
    return description_;
}
const std::vector<ProductAttribute>& Product::attributes() const noexcept {
    return attributes_;
}
const std::vector<ProductVariant>& Product::variants() const noexcept {
    return variants_;
}
ProductStatus Product::status() const noexcept {
    return status_;
}

void Product::addVariant(const ProductVariant& variant) {
    detail::check<CatalogRuleException>(status_ != ProductStatus::Archived &&
                                            variant.conformsTo(attributes_),
                                        "Вариант не соответствует состоянию или схеме продукта");
    for (const auto& existing : variants_) {
        detail::check<DuplicateEntityException>(existing.sku() != variant.sku() &&
                                                    existing.attributes() != variant.attributes(),
                                                "Повторный SKU или набор характеристик");
    }
    variants_.push_back(variant);
}

void Product::replaceAttributeSchema(std::vector<ProductAttribute> schema) {
    detail::check<CatalogRuleException>(status_ == ProductStatus::Draft,
                                        "Схема изменяется только в черновике");
    checkSchema(schema);
    detail::check<CatalogRuleException>(
        std::all_of(variants_.begin(), variants_.end(),
                    [&schema](const auto& variant) { return variant.conformsTo(schema); }),
        "Новая схема несовместима с вариантами");
    attributes_.swap(schema);
}

void Product::publish() {
    detail::check<CatalogRuleException>(
        status_ == ProductStatus::Draft &&
            std::any_of(variants_.begin(), variants_.end(),
                        [](const auto& variant) { return variant.saleEnabled(); }),
        "Публикация требует черновика с доступным вариантом");
    status_ = ProductStatus::Published;
}
void Product::archive() {
    detail::check<CatalogRuleException>(status_ != ProductStatus::Archived,
                                        "Продукт уже архивирован");
    status_ = ProductStatus::Archived;
}
void Product::restoreDraft() {
    detail::check<CatalogRuleException>(status_ == ProductStatus::Archived, "Продукт не в архиве");
    status_ = ProductStatus::Draft;
}

} // namespace shop
