#include "shop/catalog/ProductCatalog.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"
#include "shop/inventory/Inventory.hpp"
#include "shop/pricing/PriceList.hpp"
#include <algorithm>

namespace shop {
ProductComparison ProductCatalog::compareProducts(const std::vector<std::string>& productIds,
                                                  const PriceList& prices,
                                                  const Inventory& inventory) const {
    detail::require(productIds.size() >= 2, "Для сравнения выберите хотя бы два продукта");
    ProductComparison result;
    for (const auto& id : productIds) {
        const auto item = product(id);
        ComparisonColumn column{item.title(), std::nullopt, {}};
        for (const auto& option : item.variants()) {
            const bool enabled = item.status() == ProductStatus::Published && option.saleEnabled();
            column.availableBySku.emplace(option.sku(),
                                          enabled ? inventory.available(option.sku()) : 0);
            const auto price = prices.entries().find(option.sku());
            if (!enabled || price == prices.entries().end()) {
                continue;
            }
            const auto amount = price->second.unitPrice();
            if (!column.priceRange) {
                column.priceRange = std::make_pair(amount, amount);
            } else {
                column.priceRange->first = std::min(column.priceRange->first, amount);
                column.priceRange->second = std::max(column.priceRange->second, amount);
            }
        }
        detail::check<DuplicateEntityException>(
            result.products.emplace(id, std::move(column)).second, "Продукт повторён в сравнении");
        for (const auto& attribute : item.attributes()) {
            auto& row = result.attributes[attribute.name()];
            row.kinds.emplace(id, attribute.kind());
            for (const auto& option : item.variants()) {
                const auto value = option.attributes().find(attribute.name());
                if (value != option.attributes().end()) {
                    row.values[id].insert(value->second);
                }
            }
        }
    }
    for (auto& [name, row] : result.attributes) {
        std::set<AttributeKind> kinds;
        for (const auto& id : productIds) {
            const auto [found, inserted] = row.kinds.try_emplace(id, std::nullopt);
            if (found->second) {
                kinds.insert(*found->second);
            }
            row.values.try_emplace(id);
        }
        row.comparable = kinds.size() <= 1;
    }
    return result;
}
} // namespace shop
