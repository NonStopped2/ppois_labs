#pragma once
#include "post/tariffs/ServiceRate.hpp"
#include "post/tariffs/TariffZone.hpp"
#include "post/tariffs/WeightBand.hpp"

#include <chrono>
#include <functional>
#include <utility>

namespace post {

// TariffPlan — набор тарифных правил: срок действия, зоны, весовые диапазоны и допуслуги.
// По адресу, массе и времени выбирает ставки для PostageCalculator, но не проводит оплату.
// Пересечения зон и весовых диапазонов запрещены; пробелы покрытия допустимы.
class TariffPlan {
public:
    TariffPlan(const std::string& id, const std::string& name, std::chrono::sys_seconds validFrom,
               std::chrono::sys_seconds validUntil, std::vector<TariffZone> zones,
               std::vector<WeightBand> weightBands, std::vector<ServiceRate> serviceRates = {});
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds validFrom() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds validUntil() const noexcept;
    [[nodiscard]] const std::vector<TariffZone>& zones() const noexcept;
    [[nodiscard]] const std::vector<WeightBand>& weightBands() const noexcept;
    [[nodiscard]] const std::vector<ServiceRate>& serviceRates() const noexcept;
    void validateStructure() const;
    [[nodiscard]] std::pair<std::reference_wrapper<const TariffZone>,
                            std::reference_wrapper<const WeightBand>>
    resolveRates(const PostalAddress& address, std::int64_t weightGrams,
                 std::chrono::sys_seconds time) const;
    [[nodiscard]] const ServiceRate& serviceRate(ServiceKind service) const;
    bool operator==(const TariffPlan& other) const = default;

private:
    std::string id_;
    std::string name_;
    std::chrono::sys_seconds validFrom_;
    std::chrono::sys_seconds validUntil_;
    std::vector<TariffZone> zones_;
    std::vector<WeightBand> weightBands_;
    std::vector<ServiceRate> serviceRates_;
};

} // namespace post
