#pragma once
#include "shop/common/TimeWindow.hpp"
#include <string>
#include <string_view>

namespace shop {
class CartLine {
  public:
    CartLine(std::string_view sku, std::int64_t quantity, Timestamp addedAt);
    const std::string& sku() const noexcept;
    std::int64_t quantity() const noexcept;
    Timestamp addedAt() const noexcept;
    bool operator==(const CartLine&) const = default;

  private:
    std::string sku_;
    std::int64_t quantity_;
    Timestamp addedAt_;
};
} // namespace shop
