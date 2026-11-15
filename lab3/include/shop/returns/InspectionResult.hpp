#pragma once

#include "shop/returns/ReturnPolicy.hpp"
#include <string>
#include <string_view>

namespace shop {
class InspectionResult {
  public:
    InspectionResult(std::string_view sku, std::int64_t receivedQuantity,
                     std::int64_t acceptedQuantity, std::int64_t restockQuantity,
                     ItemCondition condition);
    const std::string& sku() const noexcept;
    std::int64_t receivedQuantity() const noexcept;
    std::int64_t acceptedQuantity() const noexcept;
    std::int64_t restockQuantity() const noexcept;
    ItemCondition condition() const noexcept;
    void assertConsistent() const;
    bool operator==(const InspectionResult&) const = default;

  private:
    std::string sku_;
    std::int64_t receivedQuantity_;
    std::int64_t acceptedQuantity_;
    std::int64_t restockQuantity_;
    ItemCondition condition_;
};
} // namespace shop
