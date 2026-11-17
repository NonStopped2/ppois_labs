#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shop::ui {
/** InputReader.
 * Разбирает кавычки и параметры строки, преобразует числа и ответы yes/no.
 */
class InputReader {
  public:
    static std::vector<std::string> tokenize(std::string_view line);
    static std::int64_t integer(std::string_view text);
    static bool yesNo(std::string_view text);
};

} // namespace shop::ui
