#pragma once
#include <chrono>
#include <cstdint>
#include <string>

namespace post {
// Остановка содержит времена предыдущего участка и обработки, а не реальные дорожные данные.
class RouteStop {
public:
    RouteStop(const std::string& branchId, std::int64_t sequence,
              std::chrono::minutes travelMinutesFromPrevious, std::chrono::minutes handlingMinutes);
    [[nodiscard]] const std::string& branchId() const noexcept;
    [[nodiscard]] std::int64_t sequence() const noexcept;
    [[nodiscard]] std::chrono::minutes travelMinutesFromPrevious() const noexcept;
    [[nodiscard]] std::chrono::minutes handlingMinutes() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds
    earliestDeparture(std::chrono::sys_seconds arrival) const;
    bool operator==(const RouteStop& other) const = default;

private:
    std::string branchId_;
    std::int64_t sequence_;
    std::chrono::minutes travelMinutesFromPrevious_;
    std::chrono::minutes handlingMinutes_;
};
} // namespace post
