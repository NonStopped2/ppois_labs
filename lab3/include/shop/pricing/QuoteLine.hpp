#pragma once

#include "shop/catalog/ProductVariant.hpp"
#include "shop/pricing/DiscountAllocation.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace shop {

/** A physical value snapshot, not a live reference to the catalog. */
struct PackageSnapshot {
    std::int64_t weightGrams = 0;
    std::array<std::int64_t, 3> dimensionsMm{};
    std::map<std::string, std::string> attributes;
    bool operator==(const PackageSnapshot&) const = default;
};

/** One immutable commercial line of a preview. */
class QuoteLine {
  public:
    QuoteLine(const ProductVariant& variant, std::string_view title, std::int64_t quantity,
              Money unitPrice, Money discount, bool returnEligible);
    const std::string& sku() const noexcept;
    const std::string& title() const noexcept;
    std::int64_t quantity() const noexcept;
    Money unitPrice() const noexcept;
    const DiscountAllocation& allocation() const noexcept;
    Money netTotal() const noexcept;
    const PackageSnapshot& packageSnapshot() const noexcept;
    bool returnEligible() const noexcept;
    bool operator==(const QuoteLine&) const = default;

  private:
    std::string sku_;
    std::string title_;
    std::int64_t quantity_;
    Money unitPrice_;
    DiscountAllocation allocation_;
    Money netTotal_;
    PackageSnapshot packageSnapshot_;
    bool returnEligible_;
};

} // namespace shop
