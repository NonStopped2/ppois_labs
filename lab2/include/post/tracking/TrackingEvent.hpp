#pragma once

#include "post/tracking/TrackingEventKind.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace post {

// Один проверенный факт обработки отправления: вид, время, отделение и описание.
// Проверяет состав события; порядок нескольких событий контролирует TrackingHistory.
class TrackingEvent {
public:
    TrackingEvent(std::int64_t sequence, TrackingEventKind kind,
                  std::chrono::sys_seconds occurredAt,
                  std::optional<std::string> branchId = std::nullopt,
                  const std::string& description = "");

    [[nodiscard]] std::int64_t sequence() const noexcept;
    [[nodiscard]] TrackingEventKind kind() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds occurredAt() const noexcept;
    [[nodiscard]] const std::optional<std::string>& branchId() const noexcept;
    [[nodiscard]] const std::string& description() const noexcept;

    void validate() const;

    bool operator==(const TrackingEvent& other) const = default;

private:
    std::int64_t sequence_;
    TrackingEventKind kind_;
    std::chrono::sys_seconds occurredAt_;
    std::optional<std::string> branchId_;
    std::string description_;
};

} // namespace post
