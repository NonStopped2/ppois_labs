#pragma once

#include "shop/common/Money.hpp"
#include "shop/common/TimeWindow.hpp"
#include <string>
#include <string_view>

namespace shop {

/** Current SKU price with explicit change time and local revision. */
class PriceEntry {
  public:
    PriceEntry(std::string_view sku, Money unitPrice, Timestamp changedAt, std::int64_t revision);
    const std::string& sku() const noexcept;
    Money unitPrice() const noexcept;
    Timestamp changedAt() const noexcept;
    std::int64_t revision() const noexcept;
    bool operator==(const PriceEntry&) const = default;

  private:
    std::string sku_;
    Money unitPrice_;
    Timestamp changedAt_;
    std::int64_t revision_;
};

} // namespace shop
