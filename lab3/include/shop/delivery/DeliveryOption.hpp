#pragma once
#include "shop/common/Address.hpp"
#include "shop/pricing/QuoteLine.hpp"
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace shop {
class DeliveryOption {
  public:
    DeliveryOption(std::string_view id, std::string_view name, std::set<std::string> postalPrefixes,
                   std::int64_t maximumWeightGrams, std::int64_t maximumDimensionMm, Money baseFee,
                   Money freeFromSubtotal);
    const std::string& id() const noexcept;
    const std::string& name() const noexcept;
    const std::set<std::string>& postalPrefixes() const noexcept;
    std::int64_t maximumWeightGrams() const noexcept;
    std::int64_t maximumDimensionMm() const noexcept;
    Money baseFee() const noexcept;
    Money freeFromSubtotal() const noexcept;
    bool accepts(const Address& destination, const std::vector<QuoteLine>& lines) const;
    Money calculateFee(Money goodsNet) const;
    bool operator==(const DeliveryOption&) const = default;

  private:
    std::string id_;
    std::string name_;
    std::set<std::string> postalPrefixes_;
    std::int64_t maximumWeightGrams_;
    std::int64_t maximumDimensionMm_;
    Money baseFee_;
    Money freeFromSubtotal_;
};
} // namespace shop
