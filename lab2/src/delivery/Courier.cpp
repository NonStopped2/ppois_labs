#include "post/delivery/Courier.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"
#include <algorithm>
#include <iterator>
#include <type_traits>

namespace post {
// Курьер — активный сотрудник соответствующей роли, а не произвольная карточка руководителя.
Courier::Courier(const Employee& employee, const std::string& vehicleId, std::int64_t maxLoadGrams)
    : employee_(employee), vehicleId_(detail::identifier(vehicleId)), maxLoadGrams_(maxLoadGrams) {
    if (employee_.role() != EmployeeRole::Courier || !employee_.active() || maxLoadGrams_ <= 0) {
        throw DeliveryAuthorizationException("Invalid courier card or capacity");
    }
}
// Собственная карточка используется вместе с актуальными полномочиями отделения при движениях.
const Employee& Courier::employee() const noexcept {
    return employee_;
}
// Идентификатор курьерского средства не является второй копией междугородней машины.
const std::string& Courier::vehicleId() const noexcept {
    return vehicleId_;
}
// Лимит ограничивает весь груз планируемого маршрута.
std::int64_t Courier::maxLoadGrams() const noexcept {
    return maxLoadGrams_;
}
// Набор должен в точности совпасть с рабочими заданиями перед стартом.
const std::set<std::string>& Courier::assignedTaskIds() const noexcept {
    return assignedTaskIds_;
}
// Назначение резервируется уже при первом задании, не позволяя раздать груз двум маршрутам.
const std::optional<std::string>& Courier::activeRouteId() const noexcept {
    return activeRouteId_;
}
// Проверяет владельца и текущее назначение, не полагаясь только на одинаковое имя сотрудника.
void Courier::requireRoute(const DeliveryRoute& route) const {
    if (route.courierId() != employee_.id() || activeRouteId_ != route.id()) {
        throw DeliveryAuthorizationException("Courier assigned to another route");
    }
    std::set<std::string> keys;
    std::transform(route.tasks().begin(), route.tasks().end(), std::inserter(keys, keys.end()),
                   [](const auto& task) { return task.id(); });
    if (keys != assignedTaskIds_) {
        throw InvalidShipmentStateException("Delivery assignment disagrees with route");
    }
}
// Физические операции используют действующую собственную смену в каноническом отделении.
void Courier::requireBranch(const DeliveryRoute& route, const PostalBranch& branch,
                            std::chrono::sys_seconds time) const {
    requireRoute(route);
    branch.authorize(employee_.id(), BranchOperation::DeliverShipment, time);
    if (branch.id() != employee_.branchId()) {
        throw DeliveryAuthorizationException("Courier belongs to another branch");
    }
}
// Вычитание остатка проверяет общий лимит без риска переполнить сумму масс.
void Courier::validateLoad(const DeliveryRoute& route, std::int64_t extraWeight) const {
    auto remaining = maxLoadGrams_;
    for (const auto& task : route.tasks()) {
        if (task.weightGrams() > remaining) {
            throw ShipmentLimitExceededException("Courier overload");
        }
        remaining -= task.weightGrams();
    }
    if (extraWeight > remaining) {
        throw ShipmentLimitExceededException("Courier overload");
    }
}
// Добавление задания и резервирование маршрута фиксируются вместе, без частичного списка ключей.
void Courier::assignTask(const DeliveryTask& task, DeliveryRoute& route) {
    if (route.courierId() != employee_.id() || (activeRouteId_ && activeRouteId_ != route.id()) ||
        assignedTaskIds_.contains(task.id())) {
        throw DeliveryAuthorizationException("Task already assigned or foreign route");
    }
    validateLoad(route, task.weightGrams());
    auto staged = *this;
    auto plan = route;
    plan.addTask(task);
    staged.assignedTaskIds_.insert(task.id());
    staged.activeRouteId_ = route.id();
    *this = std::move(staged);
    route = std::move(plan);
}
// Проверки и выдача выполняются только на подготовленных общих копиях.
void Courier::startTasks(DeliveryRoute& route, PostalBranch& branch, ShipmentRegistry& registry,
                         std::chrono::sys_seconds time) const {
    for (auto& task : route.tasks_) {
        task.validateShipment(registry.at(task.shipmentId()));
        if (task.state_ != DeliveryTaskState::Scheduled || task.courierId_ != employee_.id() ||
            time > task.scheduledWindow_.second) {
            throw InvalidShipmentStateException("Delivery task not ready at departure");
        }
        branch.sendForDelivery(registry, task.shipmentId(), employee_.id(), time);
        task.state_ = DeliveryTaskState::InProgress;
    }
    route.startedAt_ = time;
}
// Запуск выдаёт все посылки на общих копиях склада/реестра, иначе не запускает ни одно задание.
void Courier::startRoute(DeliveryRoute& route, PostalBranch& branch, ShipmentRegistry& registry,
                         std::chrono::sys_seconds time) {
    requireBranch(route, branch, time);
    if (route.startedAt_ || route.finishedAt_ || route.tasks_.empty()) {
        throw InvalidShipmentStateException("Delivery route cannot start");
    }
    validateLoad(route);
    auto staged = route;
    auto station = branch;
    auto shipments = registry;
    startTasks(staged, station, shipments, time);
    static_assert(std::is_nothrow_move_assignable_v<PostalBranch> &&
                  std::is_nothrow_move_assignable_v<DeliveryRoute>);
    registry = std::move(shipments);
    branch = std::move(station);
    route = std::move(staged);
}
// Дата информационного факта запрещает перепланировать отменённое задание задним числом.
void Courier::cancelRoute(DeliveryRoute& route, const PostalBranch& branch,
                          ShipmentRegistry& registry, std::chrono::sys_seconds time) {
    requireBranch(route, branch, time);
    if (route.startedAt_ || route.finishedAt_) {
        throw InvalidShipmentStateException("Only an unstarted route can be cancelled");
    }
    auto staged = route;
    auto shipments = registry;
    for (auto& task : staged.tasks_) {
        detail::appendFact(shipments.at(task.shipmentId()), TrackingEventKind::Information, time,
                           branch.id(), "Курьерское назначение отменено до выезда");
        task.state_ = DeliveryTaskState::Cancelled;
    }
    staged.finishedAt_ = time;
    staged.currentTaskIndex_ = staged.tasks_.size();
    registry = std::move(shipments);
    route = std::move(staged);
    releaseRoute();
}
// Прерывание возвращает также груз без состоявшейся попытки; журнал попыток не дополняется.
void Courier::returnTasks(DeliveryRoute& route, PostalBranch& branch, ShipmentRegistry& registry,
                          const StoragePolicy& policy, std::chrono::sys_seconds time,
                          bool interrupted) const {
    for (auto& task : route.tasks_) {
        const auto& shipment = registry.at(task.shipmentId());
        task.validateShipment(shipment);
        if (shipment.history().events().empty() ||
            time < shipment.history().events().back().occurredAt()) {
            throw InvalidShipmentStateException("Route finish precedes shipment history");
        }
        if (task.state_ == DeliveryTaskState::Failed ||
            (interrupted && task.state_ == DeliveryTaskState::InProgress)) {
            branch.returnFromCourier(registry, task.shipmentId(), employee_.id(), policy, time,
                                     task.state_ == DeliveryTaskState::InProgress);
            task.state_ = DeliveryTaskState::ReturnedToBranch;
        }
    }
}
// Общая фиксация сохраняет все четыре объекта, если хотя бы один груз не удалось разместить.
void Courier::closeRoute(DeliveryRoute& route, PostalBranch& branch, ShipmentRegistry& registry,
                         const StoragePolicy& policy, std::chrono::sys_seconds time,
                         bool interrupted) {
    requireBranch(route, branch, time);
    if (!route.startedAt_ || time < *route.startedAt_ ||
        (route.finishedAt_ && (interrupted || time < *route.finishedAt_))) {
        throw InvalidShipmentStateException("Delivery route cannot finish here");
    }
    auto staged = route;
    auto station = branch;
    auto shipments = registry;
    returnTasks(staged, station, shipments, policy, time, interrupted);
    if (!staged.finishedAt_) {
        staged.complete(time);
    }
    registry = std::move(shipments);
    branch = std::move(station);
    route = std::move(staged);
    releaseRoute();
}
// Обычный финиш по-прежнему требует обработать все задания, не скрывая пропущенных доставок.
void Courier::finishRoute(DeliveryRoute& route, PostalBranch& branch, ShipmentRegistry& registry,
                          const StoragePolicy& policy, std::chrono::sys_seconds time) {
    closeRoute(route, branch, registry, policy, time, false);
}
// Явная административная операция доступна и после истечения окна, но не обходит смену и склад.
void Courier::interruptRoute(DeliveryRoute& route, PostalBranch& branch, ShipmentRegistry& registry,
                             const StoragePolicy& policy, std::chrono::sys_seconds time) {
    closeRoute(route, branch, registry, policy, time, true);
}
// Освобождение не выделяет память и выполняется только после успешной фиксации груза.
void Courier::releaseRoute() noexcept {
    assignedTaskIds_.clear();
    activeRouteId_.reset();
}
} // namespace post
