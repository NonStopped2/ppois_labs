#pragma once
#include "post/core/PostalAddress.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace post {

// Непересекающиеся префиксы шестизначных индексов и коэффициент цены в базисных пунктах.
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
