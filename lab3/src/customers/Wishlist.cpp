#include "shop/customers/Wishlist.hpp"
#include "shop/common/CatalogRuleException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {

Wishlist::Wishlist(std::string_view customerId)
    : customerId_(detail::text(customerId, "покупатель")) {}
const std::string& Wishlist::customerId() const noexcept {
    return customerId_;
}
const std::set<std::string>& Wishlist::productIds() const noexcept {
    return productIds_;
}

void Wishlist::addProduct(const Product& product) {
    detail::check<CatalogRuleException>(product.status() == ProductStatus::Published,
                                        "В избранное добавляются опубликованные продукты");
    detail::check<DuplicateEntityException>(productIds_.insert(product.id()).second,
                                            "Продукт уже в избранном");
}
void Wishlist::removeProduct(std::string_view productId) {
    detail::check<EntityNotFoundException>(productIds_.contains(std::string(productId)),
                                           "Продукта нет в избранном");
    productIds_.erase(std::string(productId));
}
std::size_t Wishlist::removeArchived(const ProductCatalog& catalog) {
    auto candidate = productIds_;
    for (const auto& id : productIds_) {
        if (catalog.product(id).status() != ProductStatus::Published) {
            candidate.erase(id);
        }
    }
    const auto removed = productIds_.size() - candidate.size();
    productIds_.swap(candidate);
    return removed;
}

std::vector<std::string>
Wishlist::createCartSelection(const ProductCatalog& catalog,
                              const std::map<std::string, std::string>& choices) const {
    for (const auto& [id, sku] : choices) {
        detail::check<CatalogRuleException>(productIds_.contains(id),
                                            "Выбран продукт вне избранного");
    }
    std::vector<std::string> result;
    for (const auto& id : productIds_) {
        const auto variants = catalog.findCompatibleVariants(id, {});
        if (variants.empty()) {
            continue;
        }
        const auto chosen = choices.find(id);
        detail::check<CatalogRuleException>(chosen != choices.end() || variants.size() == 1,
                                            "Для продукта с вариантами явно выберите SKU");
        const auto sku = chosen == choices.end() ? variants.front().sku() : chosen->second;
        detail::check<CatalogRuleException>(
            std::any_of(variants.begin(), variants.end(),
                        [&sku](const auto& variant) { return variant.sku() == sku; }),
            "SKU не относится к доступным вариантам продукта");
        result.push_back(sku);
    }
    return result;
}

} // namespace shop
