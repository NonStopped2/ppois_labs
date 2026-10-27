#pragma once
#include "post/transport/TransportState.hpp"
#include "post/transport/VehicleType.hpp"
#include <chrono>
#include <optional>
#include <string>

namespace post {
class TransportTrip;
class VehicleInspection;
// Машина хранит назначение, а не вторую копию рейса; вместимость проверяется до резервирования.
class Vehicle {
public:
    Vehicle(const std::string& id, const std::string& registrationNumber,
            std::int64_t maxWeightGrams, std::int64_t maxBags, VehicleType type);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& registrationNumber() const noexcept;
    [[nodiscard]] std::int64_t maxWeightGrams() const noexcept;
    [[nodiscard]] std::int64_t maxBags() const noexcept;
    [[nodiscard]] VehicleState state() const noexcept;
    [[nodiscard]] const std::optional<std::string>& assignedTripId() const noexcept;
    [[nodiscard]] VehicleType type() const noexcept;
    void assignTo(const TransportTrip& trip, const VehicleInspection& inspection,
                  std::chrono::sys_seconds time);
    void releaseFrom(const TransportTrip& trip);
    bool operator==(const Vehicle& other) const = default;

private:
    friend class TransportTrip;
    void validateTrip(const TransportTrip& trip, const VehicleInspection& inspection,
                      std::chrono::sys_seconds time) const;
    std::string id_;
    std::string registrationNumber_;
    std::int64_t maxWeightGrams_;
    std::int64_t maxBags_;
    VehicleState state_{VehicleState::Available};
    std::optional<std::string> assignedTripId_;
    VehicleType type_;
};
} // namespace post
