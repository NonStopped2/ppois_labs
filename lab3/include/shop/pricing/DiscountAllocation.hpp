#pragma once

#include "shop/common/Money.hpp"
#include <cstdint>
#include <string>
#include <string_view>

namespace shop {

/** Frozen line discount; consecutive unit ranges account for every kopeck exactly once. */
class DiscountAllocation {
  public:
    DiscountAllocation(std::string_view sku, std::int64_t quantity, Money grossTotal,
                       Money discountTotal);
    const std::string& sku() const noexcept;
    std::int64_t quantity() const noexcept;
    Money grossTotal() const noexcept;
    Money discountTotal() const noexcept;
    Money netTotal() const;
    Money netForUnitRange(std::int64_t offset, std::int64_t count) const;
    bool operator==(const DiscountAllocation&) const = default;

  private:
    std::string sku_;
    std::int64_t quantity_;
    Money grossTotal_;
    Money discountTotal_;
};

} // namespace shop
