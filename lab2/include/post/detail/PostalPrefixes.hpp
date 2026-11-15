#pragma once
#include "post/detail/Validation.hpp"
#include <algorithm>
#include <iterator>
#include <vector>

namespace post::detail {
// Общая проверка почтового покрытия; префиксы остаются строками с ведущими нулями.
inline std::vector<std::string> postalPrefixes(std::vector<std::string> values) {
    constexpr std::size_t codeLength = 6;
    if (values.empty()) {
        throw std::invalid_argument("Postal coverage cannot be empty");
    }
    for (auto current = values.begin(); current != values.end(); ++current) {
        *current = normalizeText(*current);
        if (current->empty() || current->size() > codeLength ||
            !std::all_of(current->begin(), current->end(), isAsciiDigit)) {
            throw std::invalid_argument("Invalid postal prefix");
        }
        if (std::any_of(std::next(current), values.end(), [&current](const auto& other) {
                const auto normalized = normalizeText(other);
                return current->starts_with(normalized) || normalized.starts_with(*current);
            })) {
            throw std::invalid_argument("Redundant postal prefix");
        }
    }
    return values;
}
// Пересекающиеся множества могут иметь разные по длине префиксы.
inline bool prefixesOverlap(const std::vector<std::string>& first,
                            const std::vector<std::string>& second) {
    return std::any_of(first.begin(), first.end(), [&second](const auto& prefix) {
        return std::any_of(second.begin(), second.end(), [&prefix](const auto& other) {
            return prefix.starts_with(other) || other.starts_with(prefix);
        });
    });
}
} // namespace post::detail
