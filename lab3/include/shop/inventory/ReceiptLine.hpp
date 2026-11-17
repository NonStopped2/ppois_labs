#pragma once
#include "shop/common/Money.hpp"
#include <string>
#include <string_view>

namespace shop {
/** ReceiptLine.
 * Фиксирует SKU, количество, закупочную цену и партию в документе прихода.
 */
class ReceiptLine {
  public:
    ReceiptLine(std::string_view sku, std::int64_t quantity, Money purchaseUnitCost,
                std::string_view batchReference);
    const std::string& sku() const noexcept;
    std::int64_t quantity() const noexcept;
    Money purchaseUnitCost() const noexcept;
    const std::string& batchReference() const noexcept;
    Money total() const;
    bool operator==(const ReceiptLine&) const = default;

  private:
    std::string sku_;
    std::int64_t quantity_;
    Money purchaseUnitCost_;
    std::string batchReference_;
};
} // namespace shop
