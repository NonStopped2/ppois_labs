#include "post/delivery/DeliveryTask.hpp"
#include "post/delivery/Courier.hpp"
#include "post/detail/DeliveryChecks.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include <algorithm>

namespace post {
// Снимок создаётся по реально хранимому отправлению с оплаченной опцией курьера.
DeliveryTask::DeliveryTask(const std::string& id, const Shipment& shipment,
                           const StorageEntry& entry)
    : id_(detail::identifier(id)), shipmentId_(shipment.trackingNumber()),
      recipient_(shipment.recipient()), address_(recipient_.address()),
      weightGrams_(shipment.weightGrams()), storageReceivedAt_(entry.receivedAt()),
      storageDeadlineAt_(entry.deadlineAt()) {
    if (shipmentId_.empty() || !shipment.options().courierDelivery() ||
        entry.shipmentId() != shipmentId_ || entry.storedWeightGrams() != weightGrams_ ||
        entry.dimensions() != shipment.dimensions() ||
        (shipment.status() != ShipmentStatus::Arrived &&
         shipment.status() != ShipmentStatus::AwaitingPickup)) {
        throw InvalidShipmentStateException("Shipment unavailable for courier task");
    }
}
// Ключ отличает задания даже для одного номера.
const std::string& DeliveryTask::id() const noexcept {
    return id_;
}
// Само отправление по-прежнему принадлежит реестру.
const std::string& DeliveryTask::shipmentId() const noexcept {
    return shipmentId_;
}
// Получатель фиксируется при создании задания.
const Customer& DeliveryTask::recipient() const noexcept {
    return recipient_;
}
// Адрес не заменяется произвольной строкой при переносе окна.
const PostalAddress& DeliveryTask::address() const noexcept {
    return address_;
}
// Масса участвует в суммарной нагрузке маршрута.
std::int64_t DeliveryTask::weightGrams() const noexcept {
    return weightGrams_;
}
// Окно имеет включённые границы, но целиком лежит до дедлайна хранения.
const DeliveryTask::Window& DeliveryTask::scheduledWindow() const noexcept {
    return scheduledWindow_;
}
// Попытки не доступны для изменения через геттер.
const std::vector<DeliveryAttempt>& DeliveryTask::attempts() const noexcept {
    return attempts_;
}
// Failed означает груз ещё у курьера, ReturnedToBranch — физический возврат.
DeliveryTaskState DeliveryTask::state() const noexcept {
    return state_;
}
// Назначение связано с идентификатором карточки курьера.
const std::string& DeliveryTask::courierId() const noexcept {
    return courierId_;
}
// Перенос не обнуляет исходный возраст цикла доставки.
std::chrono::sys_seconds DeliveryTask::storageReceivedAt() const noexcept {
    return storageReceivedAt_;
}
// Новый складской интервал не продлевает ранее обещанный срок задания.
std::chrono::sys_seconds DeliveryTask::storageDeadlineAt() const noexcept {
    return storageDeadlineAt_;
}
// Выявляет подмену номера, массы или получателя до изменения реестра.
void DeliveryTask::validateShipment(const Shipment& shipment) const {
    // Старый снимок задания и новый пустой объект не могут сбросить накопленные неудачи.
    const auto failures = std::count_if(
        shipment.history().events().begin(), shipment.history().events().end(),
        [](const auto& fact) { return fact.kind() == TrackingEventKind::DeliveryFailed; });
    const auto recorded =
        std::count_if(attempts_.begin(), attempts_.end(), [](const auto& attempt) {
            return attempt.outcome() == DeliveryOutcome::Failed;
        });
    if (failures != recorded) {
        throw InvalidShipmentStateException("Stale delivery attempt journal");
    }
    if (shipment.trackingNumber() != shipmentId_ || shipment.weightGrams() != weightGrams_ ||
        shipment.recipient() != recipient_ || !shipment.options().courierDelivery()) {
        throw InvalidShipmentStateException("Task shipment snapshot mismatch");
    }
}
// Проверяет обе границы окна и оставшийся ресурс попыток, а не только его начало.
void DeliveryTask::validateWindow(Window window, const Courier& courier, const Shipment& shipment,
                                  const StoragePolicy& policy) const {
    validateShipment(shipment);
    detail::requireTime(window.first);
    detail::requireTime(window.second);
    courier.employee().authorize(BranchOperation::DeliverShipment, courier.employee().branchId());
    const auto last =
        attempts_.empty() ? std::nullopt : std::optional{attempts_.back().attemptedAt()};
    if ((shipment.status() != ShipmentStatus::Arrived &&
         shipment.status() != ShipmentStatus::AwaitingPickup) ||
        shipment.history().lastKnownBranch() != courier.employee().branchId() ||
        window.second < window.first ||
        window.first < shipment.history().events().back().occurredAt() ||
        window.second >= storageDeadlineAt_ ||
        attempts_.size() >= static_cast<std::uint64_t>(policy.maxDeliveryAttempts()) ||
        !policy.canAttemptDelivery(storageReceivedAt_, window.first,
                                   static_cast<std::int64_t>(attempts_.size()), last) ||
        !policy.canAttemptDelivery(storageReceivedAt_, window.second,
                                   static_cast<std::int64_t>(attempts_.size()), last)) {
        throw InvalidShipmentStateException("Invalid courier window or attempt limit");
    }
}
// Первая запись окна готовится до замены идентификатора курьера.
void DeliveryTask::schedule(Window window, const Courier& courier, const Shipment& shipment,
                            const StoragePolicy& policy) {
    // После физического выезда новый объект не должен продлевать исходный цикл хранения.
    if (state_ != DeliveryTaskState::Draft ||
        std::any_of(
            shipment.history().events().begin(), shipment.history().events().end(),
            [](const auto& fact) { return fact.kind() == TrackingEventKind::OutForDelivery; })) {
        throw InvalidShipmentStateException("Use the original task after courier departure");
    }
    validateWindow(window, courier, shipment, policy);
    auto actor = courier.employee().id();
    courierId_.swap(actor);
    scheduledWindow_ = window;
    state_ = DeliveryTaskState::Scheduled;
}
// Одна попытка должна относиться к активному назначению, окну, хронологии и политике.
void DeliveryTask::validateAttempt(const Shipment& shipment, const Courier& courier,
                                   const PostalBranch& branch, const DeliveryAttempt& attempt,
                                   const StoragePolicy& policy) const {
    validateShipment(shipment);
    attempt.validateOutcome();
    branch.authorize(courier.employee().id(), BranchOperation::DeliverShipment,
                     attempt.attemptedAt());
    const auto last =
        attempts_.empty() ? std::nullopt : std::optional{attempts_.back().attemptedAt()};
    if (state_ != DeliveryTaskState::InProgress ||
        shipment.status() != ShipmentStatus::OutForDelivery ||
        courier.employee().id() != courierId_ || !courier.activeRouteId() ||
        !courier.assignedTaskIds().contains(id_) || branch.id() != courier.employee().branchId() ||
        attempts_.size() >= static_cast<std::uint64_t>(policy.maxDeliveryAttempts()) ||
        shipment.history().lastKnownBranch() != courier.employee().branchId() ||
        attempt.attemptedAt() < scheduledWindow_.first ||
        attempt.attemptedAt() > scheduledWindow_.second ||
        attempt.attemptedAt() >= storageDeadlineAt_ ||
        std::any_of(attempts_.begin(), attempts_.end(),
                    [&attempt](const auto& prior) { return prior.id() == attempt.id(); }) ||
        !policy.canAttemptDelivery(storageReceivedAt_, attempt.attemptedAt(),
                                   static_cast<std::int64_t>(attempts_.size()), last)) {
        throw InvalidShipmentStateException("Attempt not allowed for current task");
    }
}
// Неудача фиксируется вместе с историей, но не симулирует немедленное возвращение в отделение.
void DeliveryTask::recordAttempt(ShipmentRegistry& registry, const Courier& courier,
                                 const PostalBranch& branch, const DeliveryAttempt& attempt,
                                 const StoragePolicy& policy) {
    validateAttempt(registry.at(shipmentId_), courier, branch, attempt, policy);
    if (attempt.outcome() != DeliveryOutcome::Failed) {
        throw InvalidShipmentStateException("Success requires authorized delivery");
    }
    auto staged = *this;
    auto shipments = registry;
    staged.attempts_.push_back(attempt);
    staged.state_ = DeliveryTaskState::Failed;
    detail::appendFact(shipments.at(shipmentId_), TrackingEventKind::DeliveryFailed,
                       attempt.attemptedAt(), courier.employee().branchId(), attempt.note());
    registry = std::move(shipments);
    *this = std::move(staged);
}
// Вручение объединяет проверку личности, успешную попытку и терминальный факт Delivered.
void DeliveryTask::completeDelivery(ShipmentRegistry& registry, const Courier& courier,
                                    const PostalBranch& branch, const DeliveryAttempt& attempt,
                                    const StoragePolicy& policy, const Customer& claimant,
                                    const IdentityDocument& document,
                                    const PickupAuthorization* authorization) {
    const auto& original = registry.at(shipmentId_);
    validateAttempt(original, courier, branch, attempt, policy);
    if (attempt.outcome() != DeliveryOutcome::Delivered) {
        throw InvalidShipmentStateException("Delivery requires successful outcome");
    }
    detail::validateRecipient(original, claimant, document, authorization, attempt.attemptedAt());
    auto staged = *this;
    auto shipments = registry;
    staged.attempts_.push_back(attempt);
    staged.state_ = DeliveryTaskState::Delivered;
    detail::appendFact(shipments.at(shipmentId_), TrackingEventKind::Delivered,
                       attempt.attemptedAt(), courier.employee().branchId());
    registry = std::move(shipments);
    *this = std::move(staged);
}
// После отмены плана или физического возврата можно задать новое окно без обнуления журнала.
void DeliveryTask::reschedule(Window window, const Courier& courier, const Shipment& shipment,
                              const StoragePolicy& policy) {
    if ((state_ != DeliveryTaskState::ReturnedToBranch && state_ != DeliveryTaskState::Cancelled) ||
        (!attempts_.empty() &&
         !attempts_.back().canRetryAfter(window.first,
                                         std::chrono::duration_cast<std::chrono::seconds>(
                                             std::chrono::hours{policy.minRetryHours()})))) {
        throw InvalidShipmentStateException("Task failure cannot be retried");
    }
    validateWindow(window, courier, shipment, policy);
    auto actor = courier.employee().id();
    courierId_.swap(actor);
    scheduledWindow_ = window;
    state_ = DeliveryTaskState::Scheduled;
}
} // namespace post
