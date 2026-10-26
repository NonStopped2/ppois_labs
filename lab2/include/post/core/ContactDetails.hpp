#pragma once

#include "post/core/ContactChannel.hpp"

#include <optional>
#include <string>
#include <vector>

namespace post {

// Проверяем формат контактов, но не существование номера и почтового ящика.
class ContactDetails {
public:
    explicit ContactDetails(const std::string& phone,
                            std::optional<std::string> email = std::nullopt,
                            ContactChannel preferredChannel = ContactChannel::Phone);

    [[nodiscard]] const std::string& phone() const noexcept;
    [[nodiscard]] const std::optional<std::string>& email() const noexcept;
    [[nodiscard]] ContactChannel preferredChannel() const noexcept;

    void validate() const;
    [[nodiscard]] ContactChannel selectChannel(const std::vector<ContactChannel>& available) const;
    bool operator==(const ContactDetails& other) const = default;

private:
    [[nodiscard]] bool supports(ContactChannel channel) const;

    std::string phone_;
    std::optional<std::string> email_;
    ContactChannel preferredChannel_;
};

} // namespace post
