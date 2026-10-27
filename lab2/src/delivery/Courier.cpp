#include "post/delivery/Courier.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"
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
}
// Добавление задания и резервирование маршрута фиксируются вместе, без частичного списка ключей.
void Courier::assignTask(const DeliveryTask& task, DeliveryRoute& route) {
    if (route.courierId() != employee_.id() || (activeRouteId_ && activeRouteId_ != route.id()) ||
        assignedTaskIds_.contains(task.id())) {
        throw DeliveryAuthorizationException("Task already assigned or foreign route");
    }
    auto remaining = maxLoadGrams_;
    for (const auto& item : route.tasks()) {
        if (item.weightGrams() > remaining) {
            throw ShipmentLimitExceededException("Courier overload");
        }
        remaining -= item.weightGrams();
    }
    if (task.weightGrams() > remaining) {
        throw ShipmentLimitExceededException("Courier overload");
    }
    auto staged = *this;
    auto plan = route;
    plan.addTask(task);
    staged.assignedTaskIds_.insert(task.id());
    staged.activeRouteId_ = route.id();
    *this = std::move(staged);
    route = std::move(plan);
}
// Запуск выдаёт все посылки на общих копиях склада/реестра, иначе не запускает ни одно задание.
void Courier::startRoute(DeliveryRoute& route, PostalBranch& branch, ShipmentRegistry& registry,
                         std::chrono::sys_seconds time) {
    requireRoute(route);
    branch.authorize(employee_.id(), BranchOperation::DeliverShipment, time);
    if (route.startedAt_ || route.tasks_.empty() || branch.id() != employee_.branchId()) {
        throw InvalidShipmentStateException("Delivery route cannot start");
    }
    std::set<std::string> keys;
    auto remaining = maxLoadGrams_;
    auto staged = route;
    auto station = branch;
    auto shipments = registry;
    for (auto& task : staged.tasks_) {
        // План — копируемое значение: перед стартом повторно проверяем нагрузку.
        if (task.weightGrams() > remaining) {
            throw ShipmentLimitExceededException("Courier overload at departure");
        }
        remaining -= task.weightGrams();
        task.validateShipment(shipments.at(task.shipmentId()));
        keys.insert(task.id());
        if (task.state_ != DeliveryTaskState::Scheduled || time > task.scheduledWindow_.second) {
            throw InvalidShipmentStateException("Delivery task not ready at departure");
        }
        station.sendForDelivery(shipments, task.shipmentId(), employee_.id(), time);
        task.state_ = DeliveryTaskState::InProgress;
    }
    if (keys != assignedTaskIds_) {
        throw InvalidShipmentStateException("Delivery assignment disagrees with route");
    }
    staged.startedAt_ = time;
    static_assert(std::is_nothrow_move_assignable_v<PostalBranch> &&
                  std::is_nothrow_move_assignable_v<DeliveryRoute>);
    registry = std::move(shipments);
    branch = std::move(station);
    route = std::move(staged);
}
// Возвращает неудачный груз и завершает все связанные объекты только после успешного размещения.
void Courier::finishRoute(DeliveryRoute& route, PostalBranch& branch, ShipmentRegistry& registry,
                          const StoragePolicy& policy, std::chrono::sys_seconds time) {
    requireRoute(route);
    branch.authorize(employee_.id(), BranchOperation::DeliverShipment, time);
    if (!route.startedAt_ || branch.id() != employee_.branchId() ||
        (route.finishedAt_ && time < *route.finishedAt_)) {
        throw InvalidShipmentStateException("Delivery route cannot finish here");
    }
    auto staged = route;
    auto station = branch;
    auto shipments = registry;
    for (auto& task : staged.tasks_) {
        if (task.state_ == DeliveryTaskState::Failed) {
            station.returnFromCourier(shipments, task.shipmentId(), employee_.id(), policy, time);
            task.state_ = DeliveryTaskState::ReturnedToBranch;
        }
    }
    if (!staged.finishedAt_) {
        staged.complete(time);
    }
    registry = std::move(shipments);
    branch = std::move(station);
    route = std::move(staged);
    assignedTaskIds_.clear();
    activeRouteId_.reset();
}
} // namespace post
