#pragma once
#include "post/shipments/DeliveryOptions.hpp"
#include "post/tariffs/RateMode.hpp"

#include <optional>

namespace post {

// Ставка одной дополнительной услуги; фиксированный и процентный режимы не смешиваются.
class ServiceRate {
public:
    ServiceRate(ServiceKind service, RateMode mode, Money fixedCharge, std::int64_t basisPoints,
                std::optional<Money> maximumCharge = std::nullopt);
    [[nodiscard]] ServiceKind service() const noexcept;
    [[nodiscard]] RateMode mode() const noexcept;
    [[nodiscard]] const Money& fixedCharge() const noexcept;
    [[nodiscard]] std::int64_t basisPoints() const noexcept;
    [[nodiscard]] const std::optional<Money>& maximumCharge() const noexcept;
    [[nodiscard]] Money calculateSurcharge(Money basis, std::int64_t roundingStepKopecks = 1) const;
    void validateApplicability(ShipmentKind kind, const DeliveryOptions& options) const;
    bool operator==(const ServiceRate& other) const = default;

private:
    ServiceKind service_;
    RateMode mode_;
    Money fixedCharge_;
    std::int64_t basisPoints_;
    std::optional<Money> maximumCharge_;
};

} // namespace post
