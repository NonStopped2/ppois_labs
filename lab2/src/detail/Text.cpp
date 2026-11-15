#include "post/detail/Text.hpp"

#include <algorithm>

namespace post::detail {

bool isAsciiDigit(char character) noexcept {
    return character >= '0' && character <= '9';
}

bool isAsciiSpace(char character) noexcept {
    constexpr std::string_view whitespace = " \t\n\r\f\v";
    return whitespace.find(character) != std::string_view::npos;
}

bool hasPrintableText(std::string_view text) noexcept {
    constexpr unsigned char firstPrintable = 0x20;
    constexpr unsigned char deleteCharacter = 0x7f;
    return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char character) {
        return character >= firstPrintable && character != deleteCharacter;
    });
}

std::string normalizeText(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    bool pendingSpace = false;
    for (const char character : text) {
        if (isAsciiSpace(character)) {
            pendingSpace = !result.empty();
        } else {
            if (pendingSpace) {
                result.push_back(' ');
                pendingSpace = false;
            }
            // Не меняем отдельные байты UTF-8: кириллица остаётся нетронутой.
            result.push_back(character);
        }
    }
    return result;
}

} // namespace post::detail
