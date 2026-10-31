#include "post/tariffs/ServiceRate.hpp"
#include "post/detail/PricingMath.hpp"
#include "post/exceptions/IncompatibleServiceException.hpp"

#include <algorithm>

// ServiceRate — правило доплаты за одну услугу: фиксированная сумма или процент от базы.
// Хранит режим, ставку и необязательный потолок суммы; проверяет применимость к отправлению.
// Калькулятор задаёт базу и шаг округления; фиксированный и процентный режимы не смешиваются.
namespace post {
// В фиксированном режиме процент нулевой, в процентном — фиксированная часть нулевая.
ServiceRate::ServiceRate(ServiceKind service, RateMode mode, Money fixedCharge,
                         std::int64_t basisPoints, std::optional<Money> maximumCharge)
    : service_(service), mode_(mode), fixedCharge_(fixedCharge), basisPoints_(basisPoints),
      maximumCharge_(maximumCharge) {
    if (service_ < ServiceKind::Registration || service_ > ServiceKind::DeliveryNotification) {
        throw IncompatibleServiceException("Not a supplementary service");
    }
    if ((mode_ != RateMode::Fixed && mode_ != RateMode::Percentage) || basisPoints_ < 0 ||
        (mode_ == RateMode::Fixed && basisPoints_ != 0) ||
        (mode_ == RateMode::Percentage && fixedCharge_ != Money{})) {
        throw std::invalid_argument("Inconsistent service rate mode");
    }
}
// Вид услуги связывает ставку с опцией отправления.
ServiceKind ServiceRate::service() const noexcept {
    return service_;
}
// Режим выбирает только одну формулу расчёта.
RateMode ServiceRate::mode() const noexcept {
    return mode_;
}
// Фиксированная часть применяется только в режиме Fixed.
const Money& ServiceRate::fixedCharge() const noexcept {
    return fixedCharge_;
}
// Процент хранится целым числом базисных пунктов, без double.
std::int64_t ServiceRate::basisPoints() const noexcept {
    return basisPoints_;
}
// Отсутствующий предел не равен нулевому пределу.
const std::optional<Money>& ServiceRate::maximumCharge() const noexcept {
    return maximumCharge_;
}
// Для процентной ставки база передаётся явно; предел применяется с учётом округления.
Money ServiceRate::calculateSurcharge(Money basis, std::int64_t roundingStepKopecks) const {
    if (mode_ == RateMode::Fixed) {
        return detail::roundCharge(fixedCharge_, roundingStepKopecks, maximumCharge_);
    }
    return detail::scaledCharge(basis, basisPoints_, roundingStepKopecks, maximumCharge_);
}
// Требует, чтобы услуга действительно была выбрана и подходила виду отправления.
void ServiceRate::validateApplicability(ShipmentKind kind, const DeliveryOptions& options) const {
    options.validateCombination(kind);
    const auto selected = options.requiredServices();
    if (std::find(selected.begin(), selected.end(), service_) == selected.end()) {
        throw IncompatibleServiceException("Service not selected by shipment");
    }
}

} // namespace post
