#pragma once
#include "post/core/PostalAddress.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace post {

// TariffZone — область доставки по префиксам почтовых индексов и её коэффициент цены.
// Определяет покрытие адреса и пересечение с другой зоной для проверки TariffPlan.
// Коэффициент задан целыми базисными пунктами: 10000 соответствует множителю 1.
class TariffZone {
public:
    TariffZone(const std::string& id, const std::string& name,
               std::vector<std::string> postalPrefixes, std::int64_t priceFactorBasisPoints);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] const std::vector<std::string>& postalPrefixes() const noexcept;
    [[nodiscard]] std::int64_t priceFactorBasisPoints() const noexcept;
    void validateCoverage() const;
    [[nodiscard]] bool covers(const PostalAddress& address) const;
    [[nodiscard]] bool overlaps(const TariffZone& other) const;
    bool operator==(const TariffZone& other) const = default;

private:
    std::string id_;
    std::string name_;
    std::vector<std::string> postalPrefixes_;
    std::int64_t priceFactorBasisPoints_;
};

} // namespace post
