#pragma once

#include <set>
#include <string>
#include <string_view>

namespace shop {

enum class AttributeKind { Text, Integer };

/** Schema for one characteristic, shared by all variants of a product. */
class ProductAttribute {
  public:
    ProductAttribute(std::string_view name, AttributeKind kind, bool required,
                     std::set<std::string> allowedValues = {});
    const std::string& name() const noexcept;
    AttributeKind kind() const noexcept;
    bool required() const noexcept;
    const std::set<std::string>& allowedValues() const noexcept;
    bool accepts(std::string_view value) const;
    bool operator==(const ProductAttribute&) const = default;

  private:
    std::string name_;
    AttributeKind kind_;
    bool required_;
    std::set<std::string> allowedValues_;
};

} // namespace shop
