#include "InputReader.hpp"
#include "InputError.hpp"
#include <cctype>
#include <charconv>

namespace shop::ui {
namespace {
bool space(char ch) {
    return std::isspace(static_cast<unsigned char>(ch));
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
