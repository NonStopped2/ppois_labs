#include "post/returns/SearchRequest.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include <algorithm>

// SearchRequest — розыск: снимок истории, ожидаемые отделения и результаты физических проверок.
// Подтверждает нахождение либо утрату; утрата согласованно отражается в реестре и складе.
// Новое событие делает снимок устаревшим; для признания утраты нужны проверки и минимальный срок.
namespace post {
// Черновик ещё не содержит даты начала и не связан с чужим отправлением.
SearchRequest::SearchRequest(const std::string& id) : id_(detail::identifier(id)) {}
// Ключ расследования не совпадает автоматически с трек-номером.
const std::string& SearchRequest::id() const noexcept {
    return id_;
}
// Связь появляется при успешном открытии.
const std::string& SearchRequest::shipmentId() const noexcept {
    return shipmentId_;
}
// Хранит идентификатор надлежащего заявителя, а не внешнюю изменяемую карточку.
const std::string& SearchRequest::applicantCustomerId() const noexcept {
    return applicantCustomerId_;
}
// У черновика нет фиктивной даты Unix-эпохи.
std::optional<std::chrono::sys_seconds> SearchRequest::openedAt() const noexcept {
    return openedAt_;
}
// Последнее известное место фиксируется вместе с версией истории.
const std::string& SearchRequest::lastKnownBranchId() const noexcept {
    return lastKnownBranchId_;
}
// Проверяемый путь не меняется из-за последующего редактирования внешнего маршрута.
const std::vector<std::string>& SearchRequest::expectedBranchIds() const noexcept {
    return expectedBranchIds_;
}
// Множество исключает повторную проверку одного отделения.
const std::set<std::string>& SearchRequest::checkedBranchIds() const noexcept {
    return checkedBranchIds_;
}
// Результаты физической проверки не обязаны совпадать с учётным наличием в ячейке.
const std::map<std::string, SearchRequest::Finding>& SearchRequest::findings() const noexcept {
    return findings_;
}
// Found и Lost окончательны для этого обращения.
SearchStatus SearchRequest::status() const noexcept {
    return status_;
}
// Дата окончания появляется только после успешного решения.
std::optional<std::chrono::sys_seconds> SearchRequest::closedAt() const noexcept {
    return closedAt_;
}
// Номер выявляет даже новое событие с тем же временем.
std::int64_t SearchRequest::openedSequence() const noexcept {
    return openedSequence_;
}
// Открывает розыск только принятого незавершённого отправления и по пути, включающему известное
// место.
void SearchRequest::open(const Shipment& shipment, const Customer& applicant,
                         const PostalRoute& route, std::chrono::sys_seconds time) {
    detail::requireTime(time);
    const auto state = shipment.status();
    if (status_ != SearchStatus::Draft || state == ShipmentStatus::Draft ||
        state == ShipmentStatus::Delivered || state == ShipmentStatus::Returned ||
        state == ShipmentStatus::Cancelled || state == ShipmentStatus::Lost ||
        time < shipment.history().events().back().occurredAt()) {
        throw InvalidShipmentStateException("Search cannot open for this shipment");
    }
    if (applicant.id() != shipment.sender().id() && applicant.id() != shipment.recipient().id()) {
        throw DeliveryAuthorizationException("Search applicant is unrelated");
    }
    auto staged = *this;
    staged.lastKnownBranchId_ = *shipment.history().lastKnownBranch();
    for (const auto& stop : route.stops()) {
        staged.expectedBranchIds_.push_back(stop.branchId());
    }
    if (std::find(staged.expectedBranchIds_.begin(), staged.expectedBranchIds_.end(),
                  staged.lastKnownBranchId_) == staged.expectedBranchIds_.end()) {
        throw InvalidShipmentStateException("Search route excludes last location");
    }
    staged.shipmentId_ = shipment.trackingNumber();
    staged.applicantCustomerId_ = applicant.id();
    staged.openedAt_ = time;
    staged.openedSequence_ = shipment.history().events().back().sequence();
    staged.status_ = SearchStatus::Open;
    *this = std::move(staged);
}
// Решения и проверки не могут идти назад относительно начала или уже записанных результатов.
void SearchRequest::requireResolutionTime(std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    if (status_ != SearchStatus::Open || time < *openedAt_ ||
        std::any_of(findings_.begin(), findings_.end(),
                    [time](const auto& entry) { return time < std::get<0>(entry.second); })) {
        throw InvalidShipmentStateException("Search not open at this time");
    }
}
// Проверяет ту же историю: одного совпадения статуса или места недостаточно после новых событий.
void SearchRequest::requireSnapshot(const Shipment& shipment) const {
    if (status_ != SearchStatus::Open || shipment.trackingNumber() != shipmentId_ ||
        shipment.history().events().empty() ||
        shipment.history().events().back().sequence() != openedSequence_) {
        throw InvalidShipmentStateException("Search snapshot is stale or unrelated");
    }
}
// Физический результат подписывает сотрудник проверяемого отделения; найденное должно быть учтено.
void SearchRequest::recordBranchCheck(const PostalBranch& branch, const std::string& employeeId,
                                      bool found, const std::string& note,
                                      std::chrono::sys_seconds time) {
    requireResolutionTime(time);
    branch.authorize(employeeId, BranchOperation::InvestigateShipment, time);
    auto explanation = detail::normalizeText(note);
    if (!detail::hasPrintableText(explanation)) {
        throw std::invalid_argument("Search check needs explanation");
    }
    if (checkedBranchIds_.contains(branch.id()) ||
        std::find(expectedBranchIds_.begin(), expectedBranchIds_.end(), branch.id()) ==
            expectedBranchIds_.end()) {
        throw InvalidShipmentStateException("Unexpected or duplicate search branch");
    }
    if (found) {
        (void)branch.storageEntry(shipmentId_);
    }
    auto staged = *this;
    staged.checkedBranchIds_.insert(branch.id());
    staged.findings_.emplace(branch.id(), Finding{time, found, std::move(explanation)});
    *this = std::move(staged);
}
// Находка требует положительный результат и согласованное фактическое размещение.
void SearchRequest::resolveFound(const Shipment& shipment, const PostalBranch& branch,
                                 std::chrono::sys_seconds time) {
    requireResolutionTime(time);
    requireSnapshot(shipment);
    const auto found = findings_.find(branch.id());
    const auto& entry = branch.storageEntry(shipmentId_);
    if (found == findings_.end() || !std::get<1>(found->second) ||
        shipment.history().lastKnownBranch() != branch.id() ||
        entry.storedWeightGrams() != shipment.weightGrams() ||
        entry.dimensions() != shipment.dimensions()) {
        throw InvalidShipmentStateException("Search has no confirmed consistent finding");
    }
    status_ = SearchStatus::Found;
    closedAt_ = time;
}
// Утрата требует полного отрицательного обхода и минимального срока, затем согласуется со складом.
void SearchRequest::closeAsLost(PostalBranch& branch, ShipmentRegistry& registry,
                                const std::string& employeeId, std::chrono::seconds minimumAge,
                                std::chrono::sys_seconds time) {
    requireResolutionTime(time);
    requireSnapshot(registry.at(shipmentId_));
    if (minimumAge.count() < 0) {
        throw std::invalid_argument("Negative search waiting period");
    }
    if (time - *openedAt_ < minimumAge || checkedBranchIds_.size() != expectedBranchIds_.size() ||
        branch.id() != lastKnownBranchId_ ||
        std::any_of(findings_.begin(), findings_.end(),
                    [](const auto& entry) { return std::get<1>(entry.second); })) {
        throw InvalidShipmentStateException("Search cannot yet confirm loss");
    }
    branch.confirmLoss(registry, shipmentId_, employeeId, time);
    status_ = SearchStatus::Lost;
    closedAt_ = time;
}
} // namespace post
