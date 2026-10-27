#pragma once
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
} // namespace post::detail
