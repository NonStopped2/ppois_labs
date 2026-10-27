#include "post/tariffs/PostageQuote.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/PaymentException.hpp"

#include <algorithm>
#include <numeric>
#include <set>

namespace post {
namespace {
// Услуга начисляется один раз, основная пересылка обязательна даже при нулевой цене.
void validateLines(const std::vector<InvoiceLine>& lines) {
    std::set<ServiceKind> selected;
    std::for_each(lines.begin(), lines.end(), [&selected](const auto& line) {
        if (!selected.insert(line.service()).second) {
            throw PaymentException("Duplicate quoted service");
        }
    });
    if (!selected.contains(ServiceKind::BasePostage)) {
        throw PaymentException("Quote requires base postage");
    }
}
} // namespace
// Сразу проверяет структуру и представимость итоговой суммы, оставляя неизменяемый снимок.
PostageQuote::PostageQuote(const std::string& id, const std::string& shipmentTrackingNumber,
                           std::int64_t shipmentRevision, const std::string& tariffPlanId,
                           std::chrono::sys_seconds createdAt, std::chrono::sys_seconds expiresAt,
                           std::vector<InvoiceLine> lines)
    : id_(detail::identifier(id)),
      shipmentTrackingNumber_(detail::identifier(shipmentTrackingNumber)),
      shipmentRevision_(shipmentRevision), tariffPlanId_(detail::identifier(tariffPlanId)),
      createdAt_(createdAt), expiresAt_(expiresAt), lines_(std::move(lines)) {
    detail::requireTime(createdAt_);
    detail::requireTime(expiresAt_);
    if (shipmentRevision_ < 0 || expiresAt_ < createdAt_) {
        throw std::invalid_argument("Invalid quote revision or period");
    }
    validateLines(lines_);
    (void)total();
}
// Идентификатор расчёта задаётся вызывающим процессом и не зависит от системных часов.
const std::string& PostageQuote::id() const noexcept {
    return id_;
}
// Трек-номер связывает снимок с единственным отправлением реестра.
const std::string& PostageQuote::shipmentTrackingNumber() const noexcept {
    return shipmentTrackingNumber_;
}
// Версия не позволяет применить расчёт после изменения массы или услуг.
std::int64_t PostageQuote::shipmentRevision() const noexcept {
    return shipmentRevision_;
}
// Применённый план сохраняется для объяснения цены.
const std::string& PostageQuote::tariffPlanId() const noexcept {
    return tariffPlanId_;
}
// Раньше момента создания предложение применять нельзя.
std::chrono::sys_seconds PostageQuote::createdAt() const noexcept {
    return createdAt_;
}
// Конечная временная граница включена.
std::chrono::sys_seconds PostageQuote::expiresAt() const noexcept {
    return expiresAt_;
}
// Строки принадлежат предложению и не меняются через геттер.
const std::vector<InvoiceLine>& PostageQuote::lines() const noexcept {
    return lines_;
}
// Складывает суммы всех строк с контролем каждого промежуточного результата.
Money PostageQuote::total() const {
    return std::accumulate(lines_.begin(), lines_.end(), Money{},
                           [](Money sum, const auto& line) { return sum.add(line.lineTotal()); });
}
// Проверяет живое отправление перед оплатой, а не только совпадение строковых идентификаторов.
void PostageQuote::validateFor(const Shipment& shipment, std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    if (shipment.trackingNumber() != shipmentTrackingNumber_ ||
        shipment.revision() != shipmentRevision_ || shipment.status() != ShipmentStatus::Draft ||
        time < createdAt_ || time > expiresAt_) {
        throw PaymentException("Quote is stale, expired or belongs to another shipment");
    }
    const auto required = shipment.options().requiredServices();
    if (lines_.size() != required.size() + 1) {
        throw PaymentException("Quoted services differ from shipment options");
    }
    const bool complete = std::all_of(required.begin(), required.end(), [this](const auto service) {
        return std::any_of(lines_.begin(), lines_.end(),
                           [service](const auto& line) { return line.service() == service; });
    });
    if (!complete) {
        throw PaymentException("Required service missing from quote");
    }
}

} // namespace post
