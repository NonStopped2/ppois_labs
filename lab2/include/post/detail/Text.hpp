#pragma once

#include <string>
#include <string_view>

// Общие функции обработки строк не являются дополнительными предметными классами.
namespace post::detail {

[[nodiscard]] bool isAsciiDigit(char character) noexcept;
[[nodiscard]] bool isAsciiSpace(char character) noexcept;
[[nodiscard]] bool hasPrintableText(std::string_view text) noexcept;
[[nodiscard]] std::string normalizeText(std::string_view text);

} // namespace post::detail
