#pragma once

#include "shop/pricing/QuoteLine.hpp"

namespace shop {
/** OrderLine.
 * Фиксирует данные купленного SKU, включая скидку, возвратность и габариты. Поддерживает
 * пересчёт доставки без текущего каталога.
 */
class OrderLine {
  public:
    explicit OrderLine(const QuoteLine& line);
    const std::string& sku() const noexcept;
    const std::string& title() const noexcept;
    std::int64_t quantity() const noexcept;
    Money unitPrice() const noexcept;
    const DiscountAllocation& allocation() const noexcept;
    Money netTotal() const noexcept;
    std::int64_t weightGrams() const noexcept;
    const std::map<std::string, std::string>& attributesSnapshot() const noexcept;
    const std::array<std::int64_t, 3>& dimensionsMm() const noexcept;
    bool returnEligible() const noexcept;
    QuoteLine asQuoteLine() const;
    bool operator==(const OrderLine&) const = default;

  private:
    std::string sku_;
    std::string title_;
    std::int64_t quantity_;
    Money unitPrice_;
    DiscountAllocation allocation_;
    Money netTotal_;
    std::int64_t weightGrams_;
    std::map<std::string, std::string> attributesSnapshot_;
    std::array<std::int64_t, 3> dimensionsMm_;
    bool returnEligible_;
};
} // namespace shop
