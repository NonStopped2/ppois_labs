#pragma once
#include "post/branches/PostalBranch.hpp"
#include "post/transport/VehicleInspection.hpp"

namespace post {
class TransportManifest;
// Владеет рабочими партиями. Склад, реестр, машина и рейс фиксируются только после полной
// подготовки.
class TransportTrip {
public:
    TransportTrip(const std::string& id, const PostalRoute& route);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const PostalRoute& route() const noexcept;
    [[nodiscard]] const std::string& vehicleId() const noexcept;
    [[nodiscard]] const std::vector<DispatchBatch>& batches() const noexcept;
    [[nodiscard]] TripState state() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> actualDeparture() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> actualArrival() const noexcept;
    [[nodiscard]] std::size_t currentStopIndex() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> lastActivityAt() const noexcept;
    [[nodiscard]] std::int64_t totalWeightGrams() const;
    [[nodiscard]] std::int64_t totalBags() const;
    void loadBatch(const DispatchBatch& batch, const Vehicle& vehicle);
    void depart(Vehicle& vehicle, const VehicleInspection& inspection,
                const TransportManifest& manifest, PostalBranch& source, ShipmentRegistry& registry,
                const SortingPlan& plan, const std::string& employeeId,
                std::chrono::sys_seconds time);
    void arriveAt(const PostalBranch& branch, std::chrono::sys_seconds time);
    std::size_t unloadFor(PostalBranch& branch, ShipmentRegistry& registry,
                          const std::string& employeeId, const StoragePolicy& policy,
                          std::chrono::sys_seconds time);
    bool operator==(const TransportTrip& other) const = default;

private:
    void validateVehicle(const Vehicle& vehicle) const;
    void requireCurrentUnloaded() const;
    std::string id_;
    PostalRoute route_;
    std::string vehicleId_;
    std::vector<DispatchBatch> batches_;
    TripState state_{TripState::Planned};
    std::optional<std::chrono::sys_seconds> actualDeparture_;
    std::optional<std::chrono::sys_seconds> actualArrival_;
    std::size_t currentStopIndex_{0};
    std::optional<std::chrono::sys_seconds> lastActivityAt_;
};
} // namespace post
