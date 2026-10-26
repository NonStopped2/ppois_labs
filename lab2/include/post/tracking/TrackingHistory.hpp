#pragma once

#include "post/tracking/TrackingEvent.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace post {

// Владеет хронологическим журналом одного отправления и не допускает нарушения порядка.
// По журналу определяет последнее отделение и длительность пребывания, не заменяя Shipment.
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
