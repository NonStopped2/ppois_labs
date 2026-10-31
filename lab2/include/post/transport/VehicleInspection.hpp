#pragma once
#include "post/personnel/Employee.hpp"
#include "post/transport/Vehicle.hpp"
#include <map>

namespace post {
// VehicleInspection — допуск конкретной машины: срок, инспектор и устранение замечаний.
// Одобрение требует подходящих полномочий и устранённых дефектов; новый дефект снимает одобрение.
// Vehicle и TransportTrip проверяют допуск перед выездом; границы срока включены.
class VehicleInspection {
public:
    VehicleInspection(const std::string& id, const Vehicle& vehicle,
                      std::chrono::sys_seconds inspectedAt, std::chrono::sys_seconds expiresAt);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& vehicleId() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds inspectedAt() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds expiresAt() const noexcept;
    [[nodiscard]] const std::string& inspectorId() const noexcept;
    [[nodiscard]] const std::map<std::string, bool>& defects() const noexcept;
    [[nodiscard]] bool approved() const noexcept;
    void addDefect(const std::string& description);
    void resolveDefect(const std::string& description);
    void approve(const Employee& inspector, std::chrono::sys_seconds time);
    void validateFor(const Vehicle& vehicle, std::chrono::sys_seconds time) const;
    bool operator==(const VehicleInspection& other) const = default;

private:
    std::string id_;
    std::string vehicleId_;
    std::chrono::sys_seconds inspectedAt_;
    std::chrono::sys_seconds expiresAt_;
    std::string inspectorId_;
    std::map<std::string, bool> defects_;
    bool approved_{false};
};
} // namespace post
