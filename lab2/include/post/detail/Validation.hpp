#pragma once

#include "post/detail/Text.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace post::detail {

// Идентификатор хранится без пробелов; проверка не подтверждает его внешнюю уникальность.
inline std::string identifier(std::string_view input) {
    auto value = normalizeText(input);
    if (!hasPrintableText(value) || value.find(' ') != std::string::npos) {
        throw std::invalid_argument("Identifier must be nonempty and contain no spaces");
    }
    return value;
}

// Во всей модели временные метки задаются явно и начинаются с эпохи Unix.
inline void requireTime(std::chrono::sys_seconds time) {
    if (time < std::chrono::sys_seconds{}) {
        throw std::invalid_argument("Negative Unix timestamp");
    }
}

// Прибавляет неотрицательный интервал без переполнения даты.
inline std::chrono::sys_seconds after(std::chrono::sys_seconds start, std::chrono::seconds span) {
    requireTime(start);
    const auto maximum = std::numeric_limits<std::chrono::seconds::rep>::max();
    if (span.count() < 0 || span.count() > maximum - start.time_since_epoch().count()) {
        throw std::overflow_error("Time interval cannot be represented");
    }
    return start + span;
}

} // namespace post::detail
