#pragma once
#include "post/detail/Validation.hpp"
#include <chrono>
#include <limits>
#include <stdexcept>

namespace post::detail {
// duration_cast сам не проверяет переполнение; сначала ограничиваем число минут.
inline std::chrono::seconds secondsFromMinutes(std::chrono::minutes value) {
    constexpr auto scale =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::minutes{1}).count();
    if (value.count() < 0 ||
        value.count() > std::numeric_limits<std::chrono::seconds::rep>::max() / scale) {
        throw std::invalid_argument("Minutes cannot be represented as nonnegative seconds");
    }
    return std::chrono::seconds{value.count() * scale};
}
// Не допускает усечения огромного Unix-времени до другого года при проверке документа.
inline std::chrono::year_month_day dateAt(std::chrono::sys_seconds time) {
    requireTime(time);
    const auto day = std::chrono::floor<std::chrono::days>(time);
    constexpr auto lastDay =
        std::chrono::sys_days{std::chrono::year::max() / std::chrono::December / std::chrono::last};
    if (day > lastDay) {
        throw std::invalid_argument("Time outside supported civil calendar");
    }
    return std::chrono::year_month_day{day};
}
} // namespace post::detail
