#include "shop/common/ContactDetails.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
namespace {
std::string normalizePhone(const std::string& value) {
    std::string result;
    for (const char ch : value) {
        if (ch != ' ' && ch != '-' && ch != '(' && ch != ')') {
            result += ch;
        }
    }
    const auto digits =
        result.starts_with('+') ? std::string_view(result).substr(1) : std::string_view(result);
    detail::require(digits.size() >= 7 && digits.size() <= 15 && detail::digits(digits),
                    "Некорректный номер телефона");
    return result;
}
} // namespace

ContactDetails::ContactDetails(std::string_view fullName, std::string_view email,
                               std::string_view phone)
    : fullName_(detail::text(fullName, "имя")), email_(detail::text(email, "email")),
      phone_(normalizePhone(detail::text(phone, "телефон"))) {
    const auto at = email_.find('@');
    detail::require(at != std::string::npos && at > 0 && at + 1 < email_.size() &&
                        email_.find('@', at + 1) == std::string::npos &&
                        email_.find(' ') == std::string::npos,
                    "Некорректный email");
}

const std::string& ContactDetails::fullName() const noexcept {
    return fullName_;
}
const std::string& ContactDetails::email() const noexcept {
    return email_;
}
const std::string& ContactDetails::phone() const noexcept {
    return phone_;
}

} // namespace shop
