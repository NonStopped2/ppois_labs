#include "InputReader.hpp"
#include "InputError.hpp"
#include <array>
#include <cctype>
#include <charconv>

namespace shop::ui {
namespace {
bool space(char ch) {
    return std::isspace(static_cast<unsigned char>(ch));
}
bool isValidUtf8(std::string_view text) {
    constexpr std::array minimumCodePoints{0x80, 0x800, 0x10000};
    std::size_t position = 0;
    while (position < text.size()) {
        const auto lead = static_cast<unsigned char>(text[position++]);
        if (lead < 0x80) {
            continue;
        }
        if (lead < 0xc2 || lead > 0xf4) {
            return false;
        }
        const int width = lead < 0xe0 ? 2 : (lead < 0xf0 ? 3 : 4);
        int codePoint = lead & (0x7f >> width);
        for (int read = 1; read < width; ++read) {
            if (position == text.size()) {
                return false;
            }
            const auto byte = static_cast<unsigned char>(text[position++]);
            if ((byte & 0xc0) != 0x80) {
                return false;
            }
            codePoint = (codePoint << 6) | (byte & 0x3f);
        }
        // Отсекаем избыточные кодировки, суррогаты UTF-16 и значения за пределами Unicode.
        if (codePoint < minimumCodePoints[width - 2] || codePoint > 0x10ffff ||
            (codePoint >= 0xd800 && codePoint <= 0xdfff)) {
            return false;
        }
    }
    return true;
}
std::string readToken(std::string_view line, std::size_t& position) {
    const bool quoted = line[position] == '"';
    if (quoted) {
        ++position;
    }
    std::string result;
    bool closed = !quoted;
    while (position < line.size()) {
        auto ch = line[position++];
        if (quoted && ch == '"') {
            closed = true;
            break;
        }
        if (!quoted && space(ch)) {
            break;
        }
        if (quoted && ch == '\\') {
            if (position == line.size()) {
                throw InputError("Незавершённая escape-последовательность");
            }
            ch = line[position++];
        }
        result += ch;
        if (result.size() > 4096) {
            throw InputError("Слишком большой параметр");
        }
    }
    if (!closed) {
        throw InputError("Незакрытая кавычка");
    }
    if (quoted && position < line.size() && !space(line[position])) {
        throw InputError("После кавычки нужен пробел");
    }
    // Проверяем значение после разбора кавычек и обратных косых черт.
    if (!isValidUtf8(result)) {
        throw InputError("Команда содержит некорректный UTF-8");
    }
    return result;
}
} // namespace
std::vector<std::string> InputReader::tokenize(std::string_view line) {
    if (line.size() > 8192) {
        throw InputError("Команда слишком длинная");
    }
    std::vector<std::string> result;
    std::size_t position = 0;
    while (position < line.size()) {
        if (space(line[position])) {
            ++position;
            continue;
        }
        if (result.empty() && line[position] == '#') {
            break;
        }
        if (result.size() >= 128) {
            throw InputError("Слишком много параметров");
        }
        result.push_back(readToken(line, position));
    }
    return result;
}
std::int64_t InputReader::integer(std::string_view text) {
    if (text.empty()) {
        throw InputError("Ожидалось непустое целое число");
    }
    std::int64_t result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw InputError("Ожидалось целое число: " + std::string(text));
    }
    return result;
}
bool InputReader::yesNo(std::string_view text) {
    if (text == "yes") {
        return true;
    }
    if (text == "no") {
        return false;
    }
    throw InputError("Ожидалось yes или no");
}

} // namespace shop::ui
