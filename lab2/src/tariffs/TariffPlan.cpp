#include "post/tariffs/TariffPlan.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/TariffNotFoundException.hpp"

#include <algorithm>

namespace post {
namespace {
// Для небольшого учебного справочника попарная проверка яснее дополнительного индекса.
template <typename Values, typename Conflict>
void rejectPairs(const Values& values, Conflict conflict) {
    for (std::size_t first = 0; first < values.size(); ++first) {
        for (std::size_t second = first + 1; second < values.size(); ++second) {
            if (conflict(values[first], values[second])) {
                throw std::invalid_argument("Conflicting tariff entries");
            }
        }
    }
}
} // namespace
// Владеет независимыми коллекциями и проверяет план целиком при создании.
TariffPlan::TariffPlan(const std::string& id, const std::string& name,
                       std::chrono::sys_seconds validFrom, std::chrono::sys_seconds validUntil,
                       std::vector<TariffZone> zones, std::vector<WeightBand> weightBands,
                       std::vector<ServiceRate> serviceRates)
    : id_(detail::identifier(id)), name_(detail::normalizeText(name)), validFrom_(validFrom),
      validUntil_(validUntil), zones_(std::move(zones)), weightBands_(std::move(weightBands)),
      serviceRates_(std::move(serviceRates)) {
    validateStructure();
}
// Идентификатор сохраняется в ценовом предложении.
const std::string& TariffPlan::id() const noexcept {
    return id_;
}
// Имя используется для объяснения выбранного тарифа.
const std::string& TariffPlan::name() const noexcept {
    return name_;
}
// Начало срока включено.
std::chrono::sys_seconds TariffPlan::validFrom() const noexcept {
    return validFrom_;
}
// Конец срока также включён.
std::chrono::sys_seconds TariffPlan::validUntil() const noexcept {
    return validUntil_;
}
// Покрытие нельзя изменить без создания и проверки нового плана.
const std::vector<TariffZone>& TariffPlan::zones() const noexcept {
    return zones_;
}
// Весовые интервалы принадлежат снимку плана.
const std::vector<WeightBand>& TariffPlan::weightBands() const noexcept {
    return weightBands_;
}
// Список дополнительных ставок может быть пустым для простой пересылки.
const std::vector<ServiceRate>& TariffPlan::serviceRates() const noexcept {
    return serviceRates_;
}
// Разрешает разрывы, но запрещает неоднозначный выбор и повтор услуг.
void TariffPlan::validateStructure() const {
    detail::requireTime(validFrom_);
    detail::requireTime(validUntil_);
    if (!detail::hasPrintableText(name_) || validUntil_ < validFrom_ || zones_.empty() ||
        weightBands_.empty()) {
        throw std::invalid_argument("Incomplete tariff plan or invalid validity period");
    }
    rejectPairs(zones_, [](const auto& first, const auto& second) {
        return first.id() == second.id() || first.overlaps(second);
    });
    rejectPairs(weightBands_,
                [](const auto& first, const auto& second) { return first.overlaps(second); });
    rejectPairs(serviceRates_, [](const auto& first, const auto& second) {
        return first.service() == second.service();
    });
}
// Возвращает ссылки, действительные пока жив неизменённый план; отсутствие ставки — явная ошибка.
std::pair<std::reference_wrapper<const TariffZone>, std::reference_wrapper<const WeightBand>>
TariffPlan::resolveRates(const PostalAddress& address, std::int64_t weightGrams,
                         std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    if (weightGrams <= 0) {
        throw std::invalid_argument("Weight must be positive");
    }
    const auto zone = std::find_if(zones_.begin(), zones_.end(),
                                   [&address](const auto& item) { return item.covers(address); });
    const auto band =
        std::find_if(weightBands_.begin(), weightBands_.end(),
                     [weightGrams](const auto& item) { return item.contains(weightGrams); });
    if (time < validFrom_ || time > validUntil_ || zone == zones_.end() ||
        band == weightBands_.end()) {
        throw TariffNotFoundException("No tariff for address, weight and date");
    }
    return {std::cref(*zone), std::cref(*band)};
}
// Поиск конкретной услуги используется общим расчётом, не подменяя проверку её выбора.
const ServiceRate& TariffPlan::serviceRate(ServiceKind service) const {
    const auto found =
        std::find_if(serviceRates_.begin(), serviceRates_.end(),
                     [service](const auto& rate) { return rate.service() == service; });
    if (found == serviceRates_.end()) {
        throw TariffNotFoundException("No rate for required service");
    }
    return *found;
}

} // namespace post
