#include "Arguments.hpp"
#include "InputError.hpp"
#include "InputReader.hpp"

namespace shop::ui {
Arguments::Arguments(const std::vector<std::string>& tokens) : tokens_(tokens) {}
std::string Arguments::take(std::string_view name) {
    if (empty()) {
        throw InputError("Не задан параметр: " + std::string(name));
    }
    return tokens_[position_++];
}
std::int64_t Arguments::number(std::string_view name) {
    return InputReader::integer(take(name));
}
bool Arguments::boolean(std::string_view name) {
    return InputReader::yesNo(take(name));
}
bool Arguments::empty() const noexcept {
    return position_ >= tokens_.size();
}
std::vector<std::string> Arguments::rest() {
    std::vector<std::string> result;
    while (!empty()) {
        result.push_back(take("параметр"));
    }
    return result;
}
void Arguments::finish() const {
    if (!empty()) {
        throw InputError("Лишние параметры команды");
    }
}
} // namespace shop::ui
