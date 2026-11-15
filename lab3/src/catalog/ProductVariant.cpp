#include "shop/catalog/ProductVariant.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {

ProductVariant::ProductVariant(std::string_view sku, std::string_view label,
                               std::map<std::string, std::string> attributes,
                               std::int64_t weightGrams, std::array<std::int64_t, 3> dimensionsMm,
                               bool saleEnabled)
    : sku_(detail::text(sku, "SKU")), label_(detail::text(label, "вариант")),
      attributes_(std::move(attributes)), weightGrams_(weightGrams), dimensionsMm_(dimensionsMm),
      saleEnabled_(saleEnabled) {
    detail::require(weightGrams > 0 && std::all_of(dimensionsMm.begin(), dimensionsMm.end(),
                                                   [](auto value) { return value > 0; }),
                    "Масса и габариты должны быть положительными");
    for (const auto& [name, value] : attributes_) {
        detail::require(detail::text(name, "характеристика") == name &&
                            detail::text(value, "значение") == value,
                        "Названия и значения характеристик должны быть нормализованы");
    }
}
const std::string& ProductVariant::sku() const noexcept {
    return sku_;
}
const std::string& ProductVariant::label() const noexcept {
    return label_;
}
const std::map<std::string, std::string>& ProductVariant::attributes() const noexcept {
    return attributes_;
}
std::int64_t ProductVariant::weightGrams() const noexcept {
    return weightGrams_;
}
const std::array<std::int64_t, 3>& ProductVariant::dimensionsMm() const noexcept {
    return dimensionsMm_;
}
bool ProductVariant::saleEnabled() const noexcept {
    return saleEnabled_;
}

bool ProductVariant::matchesAttributes(const std::map<std::string, std::string>& selection) const {
    return std::all_of(selection.begin(), selection.end(), [this](const auto& pair) {
        const auto found = attributes_.find(pair.first);
        return found != attributes_.end() && found->second == pair.second;
    });
}

bool ProductVariant::conformsTo(const std::vector<ProductAttribute>& schema) const {
    for (const auto& attribute : schema) {
        const auto found = attributes_.find(attribute.name());
        if (!attribute.accepts(found == attributes_.end() ? std::string_view{} : found->second)) {
            return false;
        }
    }
    return std::all_of(attributes_.begin(), attributes_.end(), [&schema](const auto& pair) {
        return std::any_of(schema.begin(), schema.end(), [&pair](const auto& attribute) {
            return attribute.name() == pair.first;
        });
    });
}

} // namespace shop
