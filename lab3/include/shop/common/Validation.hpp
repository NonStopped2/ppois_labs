#pragma once

#include "shop/common/InvalidDomainDataException.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace shop::detail {

inline constexpr std::size_t MaximumTextLength = 1024;

inline std::string text(std::string_view value, std::string_view field, bool optional = false) {
    const auto first = value.find_first_not_of(' ');
    const auto last = value.find_last_not_of(' ');
    const std::string result = first == std::string_view::npos
                                   ? std::string{}
                                   : std::string(value.substr(first, last - first + 1));
    if ((!optional && result.empty()) || result.size() > MaximumTextLength ||
        std::any_of(result.begin(), result.end(), [](unsigned char ch) { return ch < 32; })) {
        throw InvalidDomainDataException("Некорректное поле: " + std::string(field));
    }
    return result;
}

inline bool digits(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(),
                                         [](unsigned char ch) { return ch >= '0' && ch <= '9'; });
}

inline void require(bool condition, std::string_view message) {
    if (!condition) {
        throw InvalidDomainDataException(std::string(message));
    }
}

inline std::int64_t addNonnegative(std::int64_t left, std::int64_t right) {
    require(left >= 0 && right >= 0 && left <= std::numeric_limits<std::int64_t>::max() - right,
            "Переполнение неотрицательной величины");
    return left + right;
}

} // namespace shop::detail
