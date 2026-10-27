#include "post/tariffs/PostageCalculator.hpp"
#include "post/detail/PricingMath.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/TariffNotFoundException.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <utility>

namespace post {
namespace {
// Порядок соответствует дополнительным ServiceKind; BasePostage оформляется отдельно.
constexpr std::array kServiceDescriptions{"Регистрация", "Объявленная ценность",
                                          "Курьерская доставка", "Хрупкая обработка",
                                          "Уведомление о вручении"};

// Структурная готовность отделена от конкретных лимитов принимающего отделения.
void requireReady(const Shipment& shipment) {
    if (shipment.trackingNumber().empty() || shipment.status() != ShipmentStatus::Draft) {
        throw InvalidShipmentStateException("Only a registered draft can be quoted");
    }
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    shipment.validateForPosting(maximum, Dimensions(maximum, maximum, maximum), maximum);
}
// Зона применяется к основной пересылке; процент объявленной ценности имеет собственную базу.
std::vector<InvoiceLine> priceLines(const TariffPlan& plan, const Shipment& shipment,
                                    std::chrono::sys_seconds time, std::int64_t roundingStep) {
    const auto [zone, band] =
        plan.resolveRates(shipment.recipient().address(), shipment.weightGrams(), time);
    const auto base = detail::scaledCharge(band.get().priceFor(shipment.weightGrams()),
                                           zone.get().priceFactorBasisPoints(), roundingStep);
    std::vector<InvoiceLine> result{InvoiceLine(ServiceKind::BasePostage, "Пересылка", 1, base)};
    for (const auto service : shipment.options().requiredServices()) {
        const auto& rate = plan.serviceRate(service);
        rate.validateApplicability(shipment.kind(), shipment.options());
        const auto basis =
            service == ServiceKind::DeclaredValue ? shipment.options().declaredValue() : base;
        result.emplace_back(service, kServiceDescriptions.at(static_cast<std::size_t>(service)), 1,
                            rate.calculateSurcharge(basis, roundingStep));
    }
    return result;
}
} // namespace
// Копирует/принимает планы по значению и запрещает неразличимые идентификаторы.
PostageCalculator::PostageCalculator(std::vector<TariffPlan> tariffPlans,
                                     std::int64_t roundingStepKopecks,
                                     std::chrono::seconds quoteLifetime)
    : tariffPlans_(std::move(tariffPlans)), roundingStepKopecks_(roundingStepKopecks),
      quoteLifetime_(quoteLifetime) {
    if (tariffPlans_.empty() || roundingStepKopecks_ <= 0 || quoteLifetime_.count() <= 0) {
        throw std::invalid_argument("Invalid calculator configuration");
    }
    std::set<std::string> ids;
    std::for_each(tariffPlans_.begin(), tariffPlans_.end(), [&ids](const auto& plan) {
        if (!ids.insert(plan.id()).second) {
            throw std::invalid_argument("Duplicate tariff plan id");
        }
    });
}
// Планы нельзя изменить без повторной проверки нового калькулятора.
const std::vector<TariffPlan>& PostageCalculator::tariffPlans() const noexcept {
    return tariffPlans_;
}
// Округление применяется к каждой строке, а не скрытым изменением общего итога.
std::int64_t PostageCalculator::roundingStepKopecks() const noexcept {
    return roundingStepKopecks_;
}
// Срок предложения дополнительно ограничивается окончанием выбранного плана.
std::chrono::seconds PostageCalculator::quoteLifetime() const noexcept {
    return quoteLifetime_;
}
// Явный выбор плана не превращает порядок коллекции в неявное тарифное правило.
PostageQuote PostageCalculator::quote(const Shipment& shipment, const std::string& tariffPlanId,
                                      const std::string& quoteId,
                                      std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    requireReady(shipment);
    const auto planId = detail::identifier(tariffPlanId);
    const auto plan = std::find_if(tariffPlans_.begin(), tariffPlans_.end(),
                                   [&planId](const auto& value) { return value.id() == planId; });
    if (plan == tariffPlans_.end()) {
        throw TariffNotFoundException("Unknown tariff plan");
    }
    auto lines = priceLines(*plan, shipment, time, roundingStepKopecks_);
    const auto span = std::min(quoteLifetime_, plan->validUntil() - time);
    return PostageQuote(quoteId, shipment.trackingNumber(), shipment.revision(), plan->id(), time,
                        time + span, std::move(lines));
}
// Неприменимые планы пропускает, но арифметические ошибки не маскирует под отсутствие тарифа.
std::vector<PostageQuote>
PostageCalculator::compareAvailablePlans(const Shipment& shipment, const std::string& quotePrefix,
                                         std::chrono::sys_seconds time) const {
    const auto prefix = detail::identifier(quotePrefix);
    std::vector<PostageQuote> result;
    for (const auto& plan : tariffPlans_) {
        try {
            result.push_back(quote(shipment, plan.id(), prefix + "-" + plan.id(), time));
        } catch (const TariffNotFoundException&) {
            continue;
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& first, const auto& second) {
        return first.total() != second.total() ? first.total() < second.total()
                                               : first.tariffPlanId() < second.tariffPlanId();
    });
    return result;
}

} // namespace post
