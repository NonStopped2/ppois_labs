#pragma once

#include "post/tracking/TrackingEvent.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace post {

// TrackingHistory — журнал одного отправления: владеет последовательностью TrackingEvent.
// Проверяет порядок номеров и времени, выбирает события за период и считает пребывание в отделении.
// Последнее известное отделение — факт истории, не доказательство физического наличия на складе.
class TrackingHistory {
public:
    [[nodiscard]] const std::vector<TrackingEvent>& events() const noexcept;

    void append(const TrackingEvent& event);
    [[nodiscard]] std::vector<TrackingEvent> eventsBetween(std::chrono::sys_seconds from,
                                                           std::chrono::sys_seconds to) const;
    [[nodiscard]] std::optional<std::string> lastKnownBranch() const;
    [[nodiscard]] std::chrono::seconds timeSpentAt(std::string_view branchId,
                                                   std::chrono::sys_seconds asOf) const;

    bool operator==(const TrackingHistory& other) const = default;

private:
    std::vector<TrackingEvent> events_;
};

} // namespace post
