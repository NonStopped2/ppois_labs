#pragma once

#include "post/core/ContactDetails.hpp"
#include "post/core/PostalAddress.hpp"

#include <string>

namespace post {

// Customer — карточка клиента: идентификатор, имя, PostalAddress и ContactDetails.
// Один клиент может быть отправителем, получателем или представителем в разных процессах.
// Данные хранятся по значению: Shipment получает независимый снимок карточки.
class Customer {
public:
    Customer(const std::string& id, const std::string& fullName, const PostalAddress& address,
             const ContactDetails& contacts);

    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& fullName() const noexcept;
    [[nodiscard]] const PostalAddress& address() const noexcept;
    [[nodiscard]] const ContactDetails& contacts() const noexcept;

    bool operator==(const Customer& other) const = default;

private:
    std::string id_;
    std::string fullName_;
    PostalAddress address_;
    ContactDetails contacts_;
};

} // namespace post
