#pragma once
#include "post/branches/PostalBranch.hpp"
#include "post/delivery/DeliveryRoute.hpp"
#include "post/personnel/Employee.hpp"
#include <chrono>
#include <cstdint>
#include <optional>
#include <set>
#include <string>

namespace post {
// Карточка курьера и назначение одного планируемого/активного маршрута; задания принадлежат
// маршруту.
class Courier {
public:
    Courier(const Employee& employee, const std::string& vehicleId, std::int64_t maxLoadGrams);
    [[nodiscard]] const Employee& employee() const noexcept;
    [[nodiscard]] const std::string& vehicleId() const noexcept;
    [[nodiscard]] std::int64_t maxLoadGrams() const noexcept;
    [[nodiscard]] const std::set<std::string>& assignedTaskIds() const noexcept;
    [[nodiscard]] const std::optional<std::string>& activeRouteId() const noexcept;
    void assignTask(const DeliveryTask& task, DeliveryRoute& route);
    void startRoute(DeliveryRoute& route, PostalBranch& branch, ShipmentRegistry& registry,
                    std::chrono::sys_seconds time);
    void finishRoute(DeliveryRoute& route, PostalBranch& branch, ShipmentRegistry& registry,
                     const StoragePolicy& policy, std::chrono::sys_seconds time);
    bool operator==(const Courier& other) const = default;

private:
    void requireRoute(const DeliveryRoute& route) const;
    Employee employee_;
    std::string vehicleId_;
    std::int64_t maxLoadGrams_;
    std::set<std::string> assignedTaskIds_;
    std::optional<std::string> activeRouteId_;
};
} // namespace post
