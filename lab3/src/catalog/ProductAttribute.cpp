#include "shop/catalog/ProductAttribute.hpp"
#include "shop/common/Validation.hpp"
#include <charconv>

namespace shop {
namespace {
bool integer(std::string_view value) {
    std::int64_t parsed{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size();
}
} // namespace

ProductAttribute::ProductAttribute(std::string_view name, AttributeKind kind, bool required,
                                   std::set<std::string> allowedValues)
    : name_(detail::text(name, "название характеристики")), kind_(kind), required_(required),
      allowedValues_(std::move(allowedValues)) {
    detail::require(kind == AttributeKind::Text || kind == AttributeKind::Integer,
                    "Неизвестный тип характеристики");
    for (const auto& value : allowedValues_) {
        detail::require(detail::text(value, "значение характеристики") == value &&
                            (kind_ != AttributeKind::Integer || integer(value)),
                        "Некорректное допустимое значение характеристики");
    }
}
const std::string& ProductAttribute::name() const noexcept {
    return name_;
}
AttributeKind ProductAttribute::kind() const noexcept {
    return kind_;
}
bool ProductAttribute::required() const noexcept {
    return required_;
}
const std::set<std::string>& ProductAttribute::allowedValues() const noexcept {
    return allowedValues_;
}

bool ProductAttribute::accepts(std::string_view value) const {
    if (value.empty()) {
        return !required_;
    }
    const bool typeMatches = kind_ == AttributeKind::Text || integer(value);
    return typeMatches && (allowedValues_.empty() || allowedValues_.contains(std::string(value)));
}

} // namespace shop
