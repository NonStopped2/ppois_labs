#include "post/returns/CompensationClaim.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/PaymentException.hpp"
#include <algorithm>
#include <set>

namespace post {
// Черновик не объявляет мнимую выплату или решение.
CompensationClaim::CompensationClaim(const std::string& id) : id_(detail::identifier(id)) {}
// Идентификатор защищает журнал кассы от повторного проведения одного требования.
const std::string& CompensationClaim::id() const noexcept {
    return id_;
}
// Трек-номер фиксируется при подаче.
const std::string& CompensationClaim::shipmentId() const noexcept {
    return shipmentId_;
}
// Заявитель должен быть отправителем либо получателем.
const std::string& CompensationClaim::claimantCustomerId() const noexcept {
    return claimantCustomerId_;
}
// Запрошенная сумма не является автоматически одобренной.
const Money& CompensationClaim::claimedAmount() const noexcept {
    return claimedAmount_;
}
// До одобрения сумма равна нулю, но выплата всё равно требует подходящий статус.
const Money& CompensationClaim::approvedAmount() const noexcept {
    return approvedAmount_;
}
// Основание определяет необходимость закрытого розыска.
CompensationReason CompensationClaim::reason() const noexcept {
    return reason_;
}
// Дата отсутствует у черновика.
std::optional<std::chrono::sys_seconds> CompensationClaim::submittedAt() const noexcept {
    return submittedAt_;
}
// Решение и проведение денег — разные стадии.
CompensationStatus CompensationClaim::status() const noexcept {
    return status_;
}
// Ссылки — учебные подтверждения, а не загрузка и проверка реальных документов.
const std::vector<std::string>& CompensationClaim::evidenceReferences() const noexcept {
    return evidenceReferences_;
}
// Пояснение решения доступно для аудита.
const std::string& CompensationClaim::decisionReason() const noexcept {
    return decisionReason_;
}
// Выплата имеет отдельную дату после решения.
std::optional<std::chrono::sys_seconds> CompensationClaim::paidAt() const noexcept {
    return paidAt_;
}
// Позволяет запретить выплату задним числом относительно одобрения.
std::optional<std::chrono::sys_seconds> CompensationClaim::decidedAt() const noexcept {
    return decidedAt_;
}
// Проверяет связь, состояние и непустые уникальные подтверждения до записи обращения.
void CompensationClaim::submit(const Shipment& shipment, const Customer& claimant, Money amount,
                               CompensationReason requestedReason,
                               std::vector<std::string> evidence, std::chrono::sys_seconds time) {
    detail::requireTime(time);
    if (status_ != CompensationStatus::Draft || amount == Money{} || evidence.empty() ||
        shipment.trackingNumber().empty() || shipment.status() == ShipmentStatus::Draft ||
        shipment.status() == ShipmentStatus::Cancelled ||
        (requestedReason != CompensationReason::Lost &&
         requestedReason != CompensationReason::Damaged) ||
        (requestedReason == CompensationReason::Lost &&
         shipment.status() != ShipmentStatus::Lost) ||
        time < shipment.history().events().back().occurredAt()) {
        throw PaymentException("Invalid compensation submission");
    }
    if (claimant.id() != shipment.sender().id() && claimant.id() != shipment.recipient().id()) {
        throw DeliveryAuthorizationException("Unrelated compensation claimant");
    }
    std::set<std::string> unique;
    for (auto& reference : evidence) {
        reference = detail::identifier(reference);
        if (!unique.insert(reference).second) {
            throw PaymentException("Duplicate compensation evidence");
        }
    }
    auto staged = *this;
    staged.shipmentId_ = shipment.trackingNumber();
    staged.claimantCustomerId_ = claimant.id();
    staged.claimedAmount_ = amount;
    staged.reason_ = requestedReason;
    staged.evidenceReferences_ = std::move(evidence);
    staged.submittedAt_ = time;
    staged.status_ = CompensationStatus::Submitted;
    *this = std::move(staged);
}
// Явный предел действует всегда; при объявленной ценности он дополнительно ограничен ею.
Money CompensationClaim::assess(const Shipment& shipment, Money limit,
                                const SearchRequest* search) const {
    if ((status_ != CompensationStatus::Submitted && status_ != CompensationStatus::Approved) ||
        shipment.trackingNumber() != shipmentId_) {
        throw PaymentException("Claim not assessable for shipment");
    }
    if (reason_ == CompensationReason::Lost &&
        (shipment.status() != ShipmentStatus::Lost || !search ||
         search->shipmentId() != shipmentId_ || search->status() != SearchStatus::Lost ||
         search->closedAt() > submittedAt_)) {
        throw PaymentException("Loss lacks prior completed search");
    }
    const auto declared = shipment.options().declaredValue();
    return std::min(claimedAmount_, declared == Money{} ? limit : std::min(limit, declared));
}
// Повторно рассчитывает предел в момент решения, не доверяя ранее показанной оценке.
void CompensationClaim::approve(const Shipment& shipment, const PostalBranch& branch,
                                const std::string& employeeId, Money amount, Money limit,
                                std::chrono::sys_seconds time, const SearchRequest* search) {
    branch.authorize(employeeId, BranchOperation::ApproveCompensation, time);
    if (status_ != CompensationStatus::Submitted || time < *submittedAt_ ||
        time < shipment.history().events().back().occurredAt() || amount == Money{} ||
        amount > assess(shipment, limit, search)) {
        throw PaymentException("Compensation cannot be approved");
    }
    std::string explanation = "Подтверждено в пределах оценки";
    decisionReason_.swap(explanation);
    approvedAmount_ = amount;
    decidedAt_ = time;
    status_ = CompensationStatus::Approved;
}
// Мотивированный отказ не меняет состояние отправления или кассы.
void CompensationClaim::reject(const PostalBranch& branch, const std::string& employeeId,
                               const std::string& explanation, std::chrono::sys_seconds time) {
    branch.authorize(employeeId, BranchOperation::ApproveCompensation, time);
    auto text = detail::normalizeText(explanation);
    if (status_ != CompensationStatus::Submitted || time < *submittedAt_ ||
        !detail::hasPrintableText(text)) {
        throw PaymentException("Invalid compensation rejection");
    }
    decisionReason_.swap(text);
    decidedAt_ = time;
    status_ = CompensationStatus::Rejected;
}
// Подтверждает только точную одобренную сумму после решения и только один раз.
void CompensationClaim::markPaid(Money amount, std::chrono::sys_seconds time) {
    detail::requireTime(time);
    if (status_ != CompensationStatus::Approved || amount != approvedAmount_ ||
        time < *decidedAt_) {
        throw PaymentException("Compensation not payable");
    }
    paidAt_ = time;
    status_ = CompensationStatus::Paid;
}
} // namespace post
