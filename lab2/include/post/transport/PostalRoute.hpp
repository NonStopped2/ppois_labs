#pragma once
#include "post/transport/RouteStop.hpp"
#include "post/transport/VehicleType.hpp"
#include <vector>

namespace post {
// Однонаправленный путь с уникальными остановками и явным ограничением общей длительности.
class PostalRoute {
public:
    PostalRoute(const std::string& id, const std::string& name, std::vector<RouteStop> stops,
                std::chrono::minutes maxDuration, std::vector<VehicleType> allowedVehicleTypes);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] const std::vector<RouteStop>& stops() const noexcept;
    [[nodiscard]] std::chrono::minutes maxDuration() const noexcept;
    [[nodiscard]] const std::vector<VehicleType>& allowedVehicleTypes() const noexcept;
    void validateSequence() const;
    [[nodiscard]] bool connects(const std::string& sourceBranchId,
                                const std::string& targetBranchId) const;
    [[nodiscard]] std::chrono::minutes estimateDuration() const;
    bool operator==(const PostalRoute& other) const = default;

private:
    std::string id_;
    std::string name_;
    std::vector<RouteStop> stops_;
    std::chrono::minutes maxDuration_;
    std::vector<VehicleType> allowedVehicleTypes_;
};
} // namespace post
