#include "post/sorting/DispatchBatch.hpp"
#include "post/branches/PostalBranch.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/TransportUnavailableException.hpp"
#include <set>

namespace post {
// У партии разные источник и назначение; время создания задаётся явно.
DispatchBatch::DispatchBatch(const std::string& id, const std::string& sourceBranchId,
                             const std::string& targetBranchId, std::chrono::sys_seconds createdAt)
    : id_(detail::identifier(id)), sourceBranchId_(detail::identifier(sourceBranchId)),
      targetBranchId_(detail::identifier(targetBranchId)), createdAt_(createdAt) {
    detail::requireTime(createdAt_);
    if (sourceBranchId_ == targetBranchId_) {
        throw std::invalid_argument("Batch cannot target its source");
    }
}
// Идентификатор партии не заменяет идентификаторы вложенных мешков.
const std::string& DispatchBatch::id() const noexcept {
    return id_;
}
// Только исходное отделение может передать партию из своего склада.
const std::string& DispatchBatch::sourceBranchId() const noexcept {
    return sourceBranchId_;
}
// Приём проверяет точное назначение.
const std::string& DispatchBatch::targetBranchId() const noexcept {
    return targetBranchId_;
}
// Запечатанные снимки не доступны для внешнего изменения.
const std::vector<MailBag>& DispatchBatch::bags() const noexcept {
    return bags_;
}
// Протокол не допускает приём ещё не отправленной партии.
BatchStatus DispatchBatch::status() const noexcept {
    return status_;
}
// Фактические движения не могут предшествовать созданию.
std::chrono::sys_seconds DispatchBatch::createdAt() const noexcept {
    return createdAt_;
}
// Проверяет назначение, пломбу, повтор мешка и повтор любого номера между мешками.
void DispatchBatch::addBag(const MailBag& bag) {
    if (status_ != BatchStatus::Draft || !bag.sealedAt() ||
        bag.destinationBranchId() != targetBranchId_) {
        throw InvalidShipmentStateException("Bag cannot join this batch");
    }
    std::set<std::string> cargo;
    for (const auto& existing : bags_) {
        if (existing.id() == bag.id()) {
            throw InvalidShipmentStateException("Duplicate bag");
        }
        for (const auto& [cargoId, weight] : existing.shipmentIds()) {
            (void)weight;
            cargo.insert(cargoId);
        }
    }
    for (const auto& [cargoId, weight] : bag.shipmentIds()) {
        (void)weight;
        if (cargo.contains(cargoId)) {
            throw InvalidShipmentStateException("Shipment appears in multiple bags");
        }
    }
    bags_.push_back(bag);
}
// Готовность — проверка комплектации, не факт физического выбытия с отдельной датой.
void DispatchBatch::prepareForDispatch(const PostalRoute& route) {
    if (status_ != BatchStatus::Draft || bags_.empty()) {
        throw InvalidShipmentStateException("Batch is not a nonempty draft");
    }
    if (!route.connects(sourceBranchId_, targetBranchId_)) {
        throw TransportUnavailableException("Route does not connect batch branches in order");
    }
    status_ = BatchStatus::Ready;
}
// Вызывается только координатором после успешной подготовки всех складских изменений.
void DispatchBatch::markDispatched() {
    if (status_ != BatchStatus::Ready) {
        throw InvalidShipmentStateException("Batch not ready");
    }
    status_ = BatchStatus::InTransit;
}
// Подтверждение требует физические факты прибытия всех номеров в реестре.
void DispatchBatch::markReceived(const PostalBranch& branch, const ShipmentRegistry& registry,
                                 std::chrono::sys_seconds time) {
    detail::requireTime(time);
    if (status_ != BatchStatus::InTransit || branch.id() != targetBranchId_ || time < createdAt_) {
        throw InvalidShipmentStateException("Batch cannot be received here");
    }
    for (const auto& bag : bags_) {
        for (const auto& [cargoId, weight] : bag.shipmentIds()) {
            const auto& shipment = registry.at(cargoId);
            const auto state = shipment.status();
            const bool arrived =
                state == ShipmentStatus::Arrived || state == ShipmentStatus::AwaitingPickup ||
                (state == ShipmentStatus::Returning &&
                 detail::lastOperationalFact(shipment) == TrackingEventKind::Arrived);
            if (!arrived || shipment.history().lastKnownBranch() != targetBranchId_ ||
                shipment.weightGrams() != weight || time < *bag.sealedAt() ||
                shipment.history().events().back().occurredAt() > time) {
                throw InvalidShipmentStateException("Batch cargo has not arrived");
            }
        }
    }
    status_ = BatchStatus::Received;
}
} // namespace post
