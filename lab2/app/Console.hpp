#pragma once
#include "post/core/Money.hpp"
#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <string>
#include <string_view>

namespace post::cli {
// Построчный ввод не оставляет хвостов в потоке; EOF отменяет незавершённую команду.
class Console {
public:
    Console(std::istream& input, std::ostream& output);
    [[nodiscard]] std::string text(std::string_view prompt);
    [[nodiscard]] std::int64_t
    integer(std::string_view prompt, std::int64_t minimum = 0,
            std::int64_t maximum = std::numeric_limits<std::int64_t>::max());
    [[nodiscard]] bool yesNo(std::string_view prompt);
    [[nodiscard]] Money money(std::string_view prompt);
    [[nodiscard]] std::chrono::year_month_day date(std::string_view prompt);
    [[nodiscard]] bool ended() const noexcept;
    std::ostream& out() noexcept;

private:
    std::istream& input_;
    std::ostream& output_;
};
[[nodiscard]] std::string trim(std::string_view value);
} // namespace post::cli
