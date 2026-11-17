#pragma once

#include <string>
#include <string_view>

namespace shop {

/** ContactDetails.
 * Хранит имя и контакты покупателя. Проверяет структуру данных и нормализует телефон.
 */
class ContactDetails {
  public:
    ContactDetails(std::string_view fullName, std::string_view email, std::string_view phone);
    const std::string& fullName() const noexcept;
    const std::string& email() const noexcept;
    const std::string& phone() const noexcept;
    bool operator==(const ContactDetails&) const = default;

  private:
    std::string fullName_;
    std::string email_;
    std::string phone_;
};

} // namespace shop
