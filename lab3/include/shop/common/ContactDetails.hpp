#pragma once

#include <string>
#include <string_view>

namespace shop {

/** Contact data, with structural validation rather than external identity checks. */
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
