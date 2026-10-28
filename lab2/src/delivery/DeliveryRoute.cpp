#include "post/delivery/DeliveryRoute.hpp"
#include "post/delivery/Courier.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include <algorithm>
#include <set>

namespace post {
// Пустой план имеет владельца, но не мнимую дату начала.
DeliveryRoute::DeliveryRoute(const std::string& id, const std::string& courierId)
    : id_(detail::identifier(id)), courierId_(detail::identifier(courierId)) {}
// Ключ связывает назначение курьера и рабочий набор заданий.
const std::string& DeliveryRoute::id() const noexcept {
    return id_;
}
// Карточка курьера не дублируется в каждом маршруте.
const std::string& DeliveryRoute::courierId() const noexcept {
    return courierId_;
}
// Рабочие задания изменяются только операциями маршрута.
const std::vector<DeliveryTask>& DeliveryRoute::tasks() const noexcept {
    return tasks_;
}
// Начало фиксируется вместе с передачей груза из отделения.
std::optional<std::chrono::sys_seconds> DeliveryRoute::startedAt() const noexcept {
    return startedAt_;
}
// Закрытие без startedAt означает отмену до выезда; задания при этом имеют Cancelled.
std::optional<std::chrono::sys_seconds> DeliveryRoute::finishedAt() const noexcept {
    return finishedAt_;
}
// Курсор обновляется выбором очередного задания.
std::size_t DeliveryRoute::currentTaskIndex() const noexcept {
    return currentTaskIndex_;
}
// Запрещает повтор задания и самого отправления, в том числе под другим ключом.
void DeliveryRoute::addTask(const DeliveryTask& task) {
    if (startedAt_ || finishedAt_ || task.state() != DeliveryTaskState::Scheduled ||
        task.courierId() != courierId_ ||
        std::any_of(tasks_.begin(), tasks_.end(), [&task](const auto& current) {
            return current.id() == task.id() || current.shipmentId() == task.shipmentId();
        })) {
        throw InvalidShipmentStateException("Task cannot join delivery route");
    }
    tasks_.push_back(task);
    currentTaskIndex_ = 0;
}
// Перестановка строится отдельно; потеря, добавление и повтор ключа не меняют исходный порядок.
void DeliveryRoute::reorder(const std::vector<std::string>& taskIds) {
    if (startedAt_ || finishedAt_ || taskIds.size() != tasks_.size()) {
        throw InvalidShipmentStateException("Delivery route cannot reorder");
    }
    std::vector<DeliveryTask> ordered;
    std::set<std::string> seen;
    for (const auto& key : taskIds) {
        const auto found = std::find_if(tasks_.begin(), tasks_.end(),
                                        [&key](const auto& task) { return task.id() == key; });
        if (found == tasks_.end() || !seen.insert(key).second) {
            throw InvalidShipmentStateException("Invalid delivery permutation");
        }
        ordered.push_back(*found);
    }
    tasks_.swap(ordered);
    currentTaskIndex_ = 0;
}
// Пропускает обработанные задания, включая неудачи, ожидающие физического возврата при финише.
const DeliveryTask* DeliveryRoute::nextPendingTask() {
    while (currentTaskIndex_ < tasks_.size() &&
           tasks_[currentTaskIndex_].state() != DeliveryTaskState::Scheduled &&
           tasks_[currentTaskIndex_].state() != DeliveryTaskState::InProgress) {
        ++currentTaskIndex_;
    }
    return currentTaskIndex_ >= tasks_.size() ? nullptr : &tasks_[currentTaskIndex_];
}
// Проверка порядка не сдвигает курсор при неудачной изменяющей операции.
DeliveryTask& DeliveryRoute::pending(const std::string& taskId) {
    if (!startedAt_ || finishedAt_) {
        throw InvalidShipmentStateException("Delivery route not active");
    }
    const auto found = std::find_if(tasks_.begin(), tasks_.end(), [](const auto& task) {
        return task.state() == DeliveryTaskState::InProgress;
    });
    if (found == tasks_.end() || found->id() != detail::identifier(taskId)) {
        throw InvalidShipmentStateException("Not the next pending delivery task");
    }
    return *found;
}
// Обёртка сохраняет единственное владение заданием и проверяет именно активный маршрут курьера.
void DeliveryRoute::recordAttempt(const std::string& taskId, ShipmentRegistry& registry,
                                  const Courier& courier, const PostalBranch& branch,
                                  const DeliveryAttempt& attempt, const StoragePolicy& policy) {
    if (courier.activeRouteId() != id_ || courier.employee().id() != courierId_) {
        throw InvalidShipmentStateException("Courier belongs to another delivery route");
    }
    pending(taskId).recordAttempt(registry, courier, branch, attempt, policy);
}
// Вручение делегирует предметную проверку заданию, не отдавая изменяемый элемент контейнера.
void DeliveryRoute::completeDelivery(const std::string& taskId, ShipmentRegistry& registry,
                                     const Courier& courier, const PostalBranch& branch,
                                     const DeliveryAttempt& attempt, const StoragePolicy& policy,
                                     const Customer& claimant, const IdentityDocument& document,
                                     const PickupAuthorization* authorization) {
    if (courier.activeRouteId() != id_ || courier.employee().id() != courierId_) {
        throw InvalidShipmentStateException("Courier belongs to another delivery route");
    }
    pending(taskId).completeDelivery(registry, courier, branch, attempt, policy, claimant, document,
                                     authorization);
}
// Неудачный груз должен сначала вернуться на склад, успешный — иметь терминальный результат.
void DeliveryRoute::complete(std::chrono::sys_seconds time) {
    detail::requireTime(time);
    if (!startedAt_ || finishedAt_ || tasks_.empty() || time < *startedAt_ ||
        std::any_of(tasks_.begin(), tasks_.end(), [time](const auto& task) {
            return (task.state() != DeliveryTaskState::Delivered &&
                    task.state() != DeliveryTaskState::ReturnedToBranch) ||
                   (!task.attempts().empty() && time < task.attempts().back().attemptedAt());
        })) {
        throw InvalidShipmentStateException(
            "Delivery route still has unprocessed tasks or invalid time");
    }
    finishedAt_ = time;
    currentTaskIndex_ = tasks_.size();
}
} // namespace post
