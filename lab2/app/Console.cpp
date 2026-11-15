#include "Console.hpp"
#include <charconv>
#include <istream>
#include <ostream>
#include <stdexcept>

namespace post::cli {
// Удаляет только внешние ASCII-пробелы, не разбирая UTF-8 имя посимвольно.
std::string trim(std::string_view value) {
    constexpr auto whitespace = " \t\r\n\v\f";
    const auto first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    return std::string(value.substr(first, value.find_last_not_of(whitespace) - first + 1));
}
// Потоки передаются снаружи: тесты подставляют строковые, приложение — cin/cout.
Console::Console(std::istream& input, std::ostream& output) : input_(input), output_(output) {}
// Ошибка потока не маскируется под пустой корректный ответ.
std::string Console::text(std::string_view prompt) {
    output_ << prompt << ": " << std::flush;
    std::string line;
    if (!std::getline(input_, line)) {
        throw std::ios_base::failure("Ввод завершён или недоступен");
    }
    return trim(line);
}
// from_chars проверяет переполнение и потребление всей строки, в отличие от чтения префикса числа.
std::int64_t Console::integer(std::string_view prompt, std::int64_t minimum, std::int64_t maximum) {
    if (minimum > maximum) {
        throw std::invalid_argument("Неверные границы ввода");
    }
    for (;;) {
        const auto line = text(prompt);
        std::int64_t result{};
        const auto parsed = std::from_chars(line.data(), line.data() + line.size(), result);
        if (parsed.ec == std::errc{} && parsed.ptr == line.data() + line.size() &&
            result >= minimum && result <= maximum) {
            return result;
        }
        output_ << "Ошибка ввода: нужно целое число от " << minimum << " до " << maximum << ".\n";
    }
}
// Числовой ответ одинаково работает с русской и английской раскладкой.
bool Console::yesNo(std::string_view prompt) {
    return integer(std::string(prompt) + " (1 — да, 0 — нет)", 0, 1) == 1;
}
// Суммы вводятся в копейках: десятичное округление не попадает в учёт.
Money Console::money(std::string_view prompt) {
    return Money(integer(std::string(prompt) + " [коп. BYN]"));
}
// Формат и календарная допустимость проверяются до передачи документа модели.
std::chrono::year_month_day Console::date(std::string_view prompt) {
    for (;;) {
        const auto line = text(std::string(prompt) + " [ГГГГ-ММ-ДД]");
        if (line.size() == 10 && line[4] == '-' && line[7] == '-') {
            int year{}, month{}, day{};
            const auto y = std::from_chars(line.data(), line.data() + 4, year);
            const auto m = std::from_chars(line.data() + 5, line.data() + 7, month);
            const auto d = std::from_chars(line.data() + 8, line.data() + 10, day);
            const std::chrono::year_month_day calendarDate{
                std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
                std::chrono::day{static_cast<unsigned>(day)}};
            if (y.ec == std::errc{} && y.ptr == line.data() + 4 && m.ec == std::errc{} &&
                m.ptr == line.data() + 7 && d.ec == std::errc{} && d.ptr == line.data() + 10 &&
                year >= 1 && month >= 1 && day >= 1 && calendarDate.ok()) {
                return calendarDate;
            }
        }
        output_ << "Ошибка ввода: недопустимая дата.\n";
    }
}
// EOF штатно закрывает сеанс; badbit означает неисправность ввода.
bool Console::ended() const noexcept {
    return input_.eof() && !input_.bad();
}
// Вывод не смешивается с бизнес-объектами.
std::ostream& Console::out() noexcept {
    return output_;
}
} // namespace post::cli
