#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace shop {

/** A tree node; only the catalog can change its position. */
class Category {
  public:
    Category(std::string_view id, std::string_view name,
             std::optional<std::string> parentId = std::nullopt);
    const std::string& id() const noexcept;
    const std::string& name() const noexcept;
    const std::optional<std::string>& parentId() const noexcept;
    bool archived() const noexcept;
    bool operator==(const Category&) const = default;

  private:
    friend class ProductCatalog;
    std::string id_;
    std::string name_;
    std::optional<std::string> parentId_;
    bool archived_ = false;
};

} // namespace shop
