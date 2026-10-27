#pragma once
#include "post/delivery/DeliveryTask.hpp"
#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace post {
// Единственный изменяемый владелец заданий маршрута; перестановка сохраняет их состав.
class DeliveryRoute {
public:
    DeliveryRoute(const std::string& id, const std::string& courierId);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& courierId() const noexcept;
    [[nodiscard]] const std::vector<DeliveryTask>& tasks() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> startedAt() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> finishedAt() const noexcept;
    [[nodiscard]] std::size_t currentTaskIndex() const noexcept;
    void addTask(const DeliveryTask& task);
    void reorder(const std::vector<std::string>& taskIds);
    [[nodiscard]] const DeliveryTask* nextPendingTask();
    void recordAttempt(const std::string& taskId, ShipmentRegistry& registry,
                       const Courier& courier, const PostalBranch& branch,
                       const DeliveryAttempt& attempt, const StoragePolicy& policy);
    void completeDelivery(const std::string& taskId, ShipmentRegistry& registry,
                          const Courier& courier, const PostalBranch& branch,
                          const DeliveryAttempt& attempt, const StoragePolicy& policy,
                          const Customer& claimant, const IdentityDocument& document,
                          const PickupAuthorization* authorization = nullptr);
    void complete(std::chrono::sys_seconds time);
    bool operator==(const DeliveryRoute& other) const = default;

private:
    friend class Courier;
    [[nodiscard]] DeliveryTask& pending(const std::string& taskId);
    std::string id_;
    std::string courierId_;
    std::vector<DeliveryTask> tasks_;
    std::optional<std::chrono::sys_seconds> startedAt_;
    std::optional<std::chrono::sys_seconds> finishedAt_;
    std::size_t currentTaskIndex_{0};
};
} // namespace post
