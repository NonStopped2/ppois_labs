#pragma once
#include <chrono>
#include <cstdint>
#include <string>

namespace post {
// RouteStop — остановка PostalRoute: отделение, порядковый номер, время дороги и обработки.
// По времени прибытия определяет самый ранний выезд после обязательной обработки груза.
// Длительности задаются учебной конфигурацией, а не получаются из реальной дорожной системы.
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
