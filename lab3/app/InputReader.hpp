#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace shop::ui {
class InputError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class InputReader {
  public:
    static std::vector<std::string> tokenize(std::string_view line);
    static std::int64_t integer(std::string_view text);
    static bool yesNo(std::string_view text);
};
/** Each handler must consume and finish its arguments before changing domain state. */
class Arguments {
  public:
    explicit Arguments(const std::vector<std::string>& tokens);
    std::string take(std::string_view name);
    std::int64_t number(std::string_view name);
    bool boolean(std::string_view name);
    bool empty() const noexcept;
    std::vector<std::string> rest();
    void finish() const;

  private:
    const std::vector<std::string>& tokens_;
    std::size_t position_ = 1;
};
} // namespace shop::ui
