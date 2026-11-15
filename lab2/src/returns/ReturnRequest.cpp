#include "post/returns/ReturnRequest.hpp"
#include "post/branches/PostalBranch.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"

// ReturnRequest — запрос возврата отправителю: инициатор, причина, решение и даты.
// Разделяет подачу, одобрение/отказ и начало возврата через PostalBranch.
// Одобрение само не перевозит груз; исходные отправитель и получатель в Shipment не меняются.
namespace post {
// Черновик ещё не связан с отправлением и не содержит мнимой даты обращения.
ReturnRequest::ReturnRequest(const std::string& id) : id_(detail::identifier(id)) {}
// Идентификатор позволяет различать независимые обращения.
const std::string& ReturnRequest::id() const noexcept {
    return id_;
}
// Номер появляется только при успешной подаче.
const std::string& ReturnRequest::shipmentId() const noexcept {
    return shipmentId_;
}
// Инициатором может быть клиент или уполномоченный сотрудник.
const std::string& ReturnRequest::initiatorId() const noexcept {
    return initiatorId_;
}
// Вид не выводится из текстового формата идентификатора.
ReturnInitiatorKind ReturnRequest::initiatorKind() const noexcept {
    return initiatorKind_;
}
// В черновике значение по умолчанию ещё не является поданным основанием.
ReturnReason ReturnRequest::reason() const noexcept {
    return reason_;
}
// Пустая дата отличает черновик от обращения в Unix-эпоху.
std::optional<std::chrono::sys_seconds> ReturnRequest::requestedAt() const noexcept {
    return requestedAt_;
}
// Решение не означает, что груз уже начал физический возврат.
ReturnStatus ReturnRequest::status() const noexcept {
    return status_;
}
// Решение имеет собственную дату, не переписывающую дату подачи.
std::optional<std::chrono::sys_seconds> ReturnRequest::decidedAt() const noexcept {
    return decidedAt_;
}
// Мотивированный отказ и подтверждение сохраняются для объяснения результата.
const std::string& ReturnRequest::decisionReason() const noexcept {
    return decisionReason_;
}
// Новое обращение возможно только по находящемуся на хранении зарегистрированному отправлению.
void ReturnRequest::validateSubmission(const Shipment& shipment,
                                       std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    if (status_ != ReturnStatus::Draft) {
        throw InvalidShipmentStateException("Return request already submitted");
    }
    detail::requireReturnable(shipment);
    if (shipment.trackingNumber().empty() ||
        time < shipment.history().events().back().occurredAt()) {
        throw InvalidShipmentStateException("Return request predates shipment history");
    }
}
// Сначала готовит обе строки, чтобы даже ошибка выделения памяти не записала половину обращения.
void ReturnRequest::recordSubmission(const Shipment& shipment, const std::string& requesterId,
                                     ReturnInitiatorKind kind, ReturnReason requestedReason,
                                     std::chrono::sys_seconds time) {
    auto staged = *this;
    staged.shipmentId_ = shipment.trackingNumber();
    staged.initiatorId_ = requesterId;
    staged.initiatorKind_ = kind;
    staged.reason_ = requestedReason;
    staged.requestedAt_ = time;
    staged.status_ = ReturnStatus::Submitted;
    *this = std::move(staged);
}
// Просьбу отправителя и отказ получателя подаёт только соответствующий клиент.
void ReturnRequest::submit(const Shipment& shipment, const Customer& customer,
                           ReturnReason requestedReason, std::chrono::sys_seconds time) {
    validateSubmission(shipment, time);
    const bool sender =
        requestedReason == ReturnReason::SenderRequest && customer.id() == shipment.sender().id();
    const bool recipient = requestedReason == ReturnReason::RecipientRefused &&
                           customer.id() == shipment.recipient().id();
    if (!sender && !recipient) {
        throw DeliveryAuthorizationException("Customer cannot submit this return reason");
    }
    recordSubmission(shipment, customer.id(), ReturnInitiatorKind::Customer, requestedReason, time);
}
// Истечение срока фиксирует сотрудник текущего отделения; само истечение проверяется при решении.
void ReturnRequest::submit(const Shipment& shipment, const Employee& employee,
                           const PostalBranch& branch, ReturnReason requestedReason,
                           std::chrono::sys_seconds time) {
    validateSubmission(shipment, time);
    employee.authorize(BranchOperation::ApproveReturn, branch.id());
    branch.authorize(employee.id(), BranchOperation::ApproveReturn, time);
    if (requestedReason != ReturnReason::StorageExpired ||
        shipment.history().lastKnownBranch() != branch.id()) {
        throw DeliveryAuthorizationException("Employee return has wrong reason or branch");
    }
    (void)branch.storageEntry(shipment.trackingNumber());
    recordSubmission(shipment, employee.id(), ReturnInitiatorKind::Employee, requestedReason, time);
}
// Решение связано с тем же грузом, физическим местом, сотрудником и актуальной хронологией.
void ReturnRequest::validateDecision(const Shipment& shipment, const Employee& employee,
                                     const PostalBranch& branch,
                                     std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    if (status_ != ReturnStatus::Submitted || shipment.trackingNumber() != shipmentId_) {
        throw InvalidShipmentStateException("Return request not awaiting this decision");
    }
    detail::requireReturnable(shipment);
    employee.authorize(BranchOperation::ApproveReturn, branch.id());
    branch.authorize(employee.id(), BranchOperation::ApproveReturn, time);
    if (time < *requestedAt_ || time < shipment.history().events().back().occurredAt() ||
        shipment.history().lastKnownBranch() != branch.id()) {
        throw InvalidShipmentStateException("Invalid return decision chronology or location");
    }
    (void)branch.storageEntry(shipmentId_);
}
// Для просрочки нужны и сохранённый срок размещения, и явно выбранная политика.
void ReturnRequest::approve(const Shipment& shipment, const Employee& employee,
                            const PostalBranch& branch, const StoragePolicy& policy,
                            std::chrono::sys_seconds time) {
    validateDecision(shipment, employee, branch, time);
    const auto& entry = branch.storageEntry(shipmentId_);
    if (reason_ == ReturnReason::StorageExpired &&
        (!entry.isOverdueAt(time) || time < policy.deadlineFrom(entry.receivedAt()))) {
        throw InvalidShipmentStateException("Storage deadline has not expired");
    }
    std::string explanation = "Основание возврата подтверждено";
    decisionReason_.swap(explanation);
    decidedAt_ = time;
    status_ = ReturnStatus::Approved;
}
// Отказ обязателен с содержательным пояснением и не меняет состояние отправления.
void ReturnRequest::reject(const Shipment& shipment, const Employee& employee,
                           const PostalBranch& branch, const std::string& rejectionReason,
                           std::chrono::sys_seconds time) {
    validateDecision(shipment, employee, branch, time);
    auto explanation = detail::normalizeText(rejectionReason);
    if (!detail::hasPrintableText(explanation)) {
        throw std::invalid_argument("Return rejection needs explanation");
    }
    decisionReason_.swap(explanation);
    decidedAt_ = time;
    status_ = ReturnStatus::Rejected;
}
// Отделение атомарно меняет реестр; только после успеха запрос помечается начатым без бросающих
// шагов.
void ReturnRequest::beginReturn(PostalBranch& branch, ShipmentRegistry& registry,
                                const Employee& employee, std::chrono::sys_seconds time) {
    detail::requireTime(time);
    if (status_ != ReturnStatus::Approved || time < *decidedAt_) {
        throw InvalidShipmentStateException("Return request has no current approval");
    }
    employee.authorize(BranchOperation::ApproveReturn, branch.id());
    // После решения груз мог пройти другое размещение с новым сроком хранения.
    if (reason_ == ReturnReason::StorageExpired &&
        !branch.storageEntry(shipmentId_).isOverdueAt(time)) {
        throw InvalidShipmentStateException("Current storage deadline has not expired");
    }
    branch.beginReturn(registry, shipmentId_, employee.id(), time);
    status_ = ReturnStatus::Started;
}
} // namespace post
