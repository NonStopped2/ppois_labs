#include "shop/catalog/ProductCatalog.hpp"
#include "shop/common/CatalogRuleException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>
#include <iterator>
#include <set>

namespace shop {

void ProductCatalog::checkSiblingName(const Category& node,
                                      const std::optional<std::string>& parentId) const {
    for (const auto& [id, existing] : categories_) {
        detail::check<DuplicateEntityException>(
            id == node.id() || existing.archived() || existing.parentId() != parentId ||
                existing.name() != node.name(),
            "Категория с таким именем уже существует у родителя");
    }
}

void ProductCatalog::addCategory(const Category& node) {
    detail::check<DuplicateEntityException>(!categories_.contains(node.id()),
                                            "Повторный ID категории");
    if (node.parentId()) {
        detail::check<CatalogRuleException>(!detail::get(categories_, *node.parentId()).archived(),
                                            "Родительская категория архивирована");
    }
    checkSiblingName(node, node.parentId());
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    categories_.emplace(node.id(), node);
    revision_ = nextRevision;
}

void ProductCatalog::addProduct(const Product& item) {
    detail::check<DuplicateEntityException>(!products_.contains(item.id()),
                                            "Повторный ID продукта");
    detail::check<CatalogRuleException>(!detail::get(categories_, item.categoryId()).archived(),
                                        "Нельзя добавить продукт в архивную категорию");
    auto candidateProducts = products_;
    auto index = skuIndex_;
    for (const auto& option : item.variants()) {
        detail::check<DuplicateEntityException>(index.emplace(option.sku(), item.id()).second,
                                                "SKU уже принадлежит другому продукту");
    }
    candidateProducts.emplace(item.id(), item);
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    products_.swap(candidateProducts);
    skuIndex_.swap(index);
    revision_ = nextRevision;
}

void ProductCatalog::replaceProduct(Product candidate) {
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    auto candidateProducts = products_;
    candidateProducts.at(candidate.id()) = std::move(candidate);
    products_.swap(candidateProducts);
    revision_ = nextRevision;
}

void ProductCatalog::addVariant(std::string_view productId, const ProductVariant& option) {
    auto candidate = product(productId);
    candidate.addVariant(option);
    auto index = skuIndex_;
    detail::check<DuplicateEntityException>(index.emplace(option.sku(), candidate.id()).second,
                                            "SKU уже существует в каталоге");
    replaceProduct(std::move(candidate));
    skuIndex_.swap(index);
}

void ProductCatalog::publish(std::string_view productId) {
    auto candidate = product(productId);
    detail::check<CatalogRuleException>(!category(candidate.categoryId()).archived(),
                                        "Категория архивирована");
    candidate.publish();
    replaceProduct(std::move(candidate));
}
void ProductCatalog::replaceAttributeSchema(std::string_view productId,
                                            std::vector<ProductAttribute> schema) {
    auto candidate = product(productId);
    candidate.replaceAttributeSchema(std::move(schema));
    replaceProduct(std::move(candidate));
}
void ProductCatalog::archiveProduct(std::string_view productId) {
    auto candidate = product(productId);
    candidate.archive();
    replaceProduct(std::move(candidate));
}
void ProductCatalog::restoreProductDraft(std::string_view productId) {
    auto candidate = product(productId);
    detail::check<CatalogRuleException>(!category(candidate.categoryId()).archived(),
                                        "Категория архивирована");
    candidate.restoreDraft();
    replaceProduct(std::move(candidate));
}

void ProductCatalog::setVariantAvailability(std::string_view sku, bool enabled) {
    auto candidate = productForSku(sku);
    detail::check<CatalogRuleException>(candidate.status_ != ProductStatus::Archived,
                                        "Архивный продукт нельзя включить в продажу");
    const auto selected = std::find_if(candidate.variants_.begin(), candidate.variants_.end(),
                                       [sku](const auto& variant) { return variant.sku() == sku; });
    selected->saleEnabled_ = enabled;
    if (candidate.status_ == ProductStatus::Published &&
        std::none_of(candidate.variants_.begin(), candidate.variants_.end(),
                     [](const auto& variant) { return variant.saleEnabled(); })) {
        candidate.status_ = ProductStatus::Draft;
    }
    replaceProduct(std::move(candidate));
}

bool ProductCatalog::isInCategory(std::string_view categoryId, std::string_view ancestorId) const {
    detail::get(categories_, ancestorId);
    auto current = category(categoryId);
    while (current.id() != ancestorId && current.parentId()) {
        current = category(*current.parentId());
    }
    return current.id() == ancestorId;
}

void ProductCatalog::moveCategory(std::string_view categoryId,
                                  std::optional<std::string> parentId) {
    auto candidate = category(categoryId);
    detail::check<CatalogRuleException>(!candidate.archived(), "Категория архивирована");
    if (parentId) {
        *parentId = detail::text(*parentId, "ID родителя");
        detail::check<CatalogRuleException>(!category(*parentId).archived() &&
                                                !isInCategory(*parentId, categoryId),
                                            "Перенос создаёт цикл или ведёт в архив");
    }
    checkSiblingName(candidate, parentId);
    candidate.parentId_ = std::move(parentId);
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    auto candidateCategories = categories_;
    candidateCategories.at(candidate.id()) = std::move(candidate);
    categories_.swap(candidateCategories);
    revision_ = nextRevision;
}

void ProductCatalog::archiveCategory(std::string_view categoryId) {
    const auto candidate = category(categoryId);
    detail::check<CatalogRuleException>(!candidate.archived(), "Категория уже архивирована");
    for (const auto& [id, child] : categories_) {
        detail::check<CatalogRuleException>(child.archived() || child.parentId() != candidate.id(),
                                            "В категории остались действующие подкатегории");
    }
    for (const auto& [id, item] : products_) {
        detail::check<CatalogRuleException>(item.categoryId() != categoryId ||
                                                item.status() == ProductStatus::Archived,
                                            "В категории остались неархивные продукты");
    }
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    categories_.at(candidate.id()).archived_ = true;
    revision_ = nextRevision;
}

void ProductCatalog::bulkMoveProducts(const std::vector<std::string>& productIds,
                                      std::string_view categoryId) {
    const auto target = category(categoryId);
    detail::check<CatalogRuleException>(!target.archived() && !productIds.empty(),
                                        "Неверная целевая категория или пустой набор");
    auto candidateProducts = products_;
    std::set<std::string> seen;
    for (const auto& id : productIds) {
        detail::get(products_, id);
        detail::check<DuplicateEntityException>(seen.insert(id).second,
                                                "Продукт повторён в переносе");
        candidateProducts.at(id).categoryId_ = target.id();
    }
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    products_.swap(candidateProducts);
    revision_ = nextRevision;
}

void ProductCatalog::mergeCategories(std::string_view sourceId, std::string_view targetId) {
    const auto source = category(sourceId);
    const auto target = category(targetId);
    detail::check<CatalogRuleException>(!source.archived() && !target.archived() &&
                                            !isInCategory(sourceId, targetId) &&
                                            !isInCategory(targetId, sourceId),
                                        "Объединение архивных или родственных категорий запрещено");
    auto candidate = *this;
    for (const auto& [id, child] : categories_) {
        if (!child.archived() && child.parentId() == source.id()) {
            candidate.moveCategory(id, target.id());
        }
    }
    std::vector<std::string> moving;
    for (const auto& [id, item] : products_) {
        if (item.categoryId() == sourceId) {
            moving.push_back(id);
        }
    }
    if (!moving.empty()) {
        candidate.bulkMoveProducts(moving, target.id());
    }
    candidate.archiveCategory(source.id());
    candidate.revision_ = detail::addNonnegative(revision_, 1);
    categories_.swap(candidate.categories_);
    products_.swap(candidate.products_);
    revision_ = candidate.revision_;
}

std::vector<Product>
ProductCatalog::search(std::string_view query, std::optional<std::string> categoryId,
                       const std::map<std::string, std::string>& attributes) const {
    if (categoryId) {
        category(*categoryId);
    }
    std::vector<Product> result;
    for (const auto& [id, item] : products_) {
        if (item.status() != ProductStatus::Published ||
            (categoryId && !isInCategory(item.categoryId(), *categoryId))) {
            continue;
        }
        const bool matchesText = item.title().find(query) != std::string::npos ||
                                 item.description().find(query) != std::string::npos;
        if (matchesText && !findCompatibleVariants(id, attributes).empty()) {
            result.push_back(item);
        }
    }
    return result;
}

std::vector<ProductVariant>
ProductCatalog::findCompatibleVariants(std::string_view productId,
                                       const std::map<std::string, std::string>& attributes) const {
    const auto value = product(productId);
    std::vector<ProductVariant> result;
    if (value.status() != ProductStatus::Published) {
        return result;
    }
    std::copy_if(value.variants().begin(), value.variants().end(), std::back_inserter(result),
                 [&attributes](const auto& option) {
                     return option.saleEnabled() && option.matchesAttributes(attributes);
                 });
    return result;
}
Product ProductCatalog::product(std::string_view id) const {
    return detail::get(products_, id);
}
Product ProductCatalog::productForSku(std::string_view sku) const {
    return product(detail::get(skuIndex_, sku));
}
ProductVariant ProductCatalog::variant(std::string_view sku) const {
    const auto value = productForSku(sku);
    const auto found = std::find_if(value.variants().begin(), value.variants().end(),
                                    [sku](const auto& variant) { return variant.sku() == sku; });
    return *found;
}
Category ProductCatalog::category(std::string_view id) const {
    return detail::get(categories_, id);
}
std::int64_t ProductCatalog::revision() const noexcept {
    return revision_;
}
const std::map<std::string, Category>& ProductCatalog::categories() const noexcept {
    return categories_;
}
const std::map<std::string, Product>& ProductCatalog::products() const noexcept {
    return products_;
}

} // namespace shop
