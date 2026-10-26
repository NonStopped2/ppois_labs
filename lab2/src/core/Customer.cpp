#include "post/core/Customer.hpp"

#include "post/detail/Text.hpp"

#include <stdexcept>

namespace post {

Customer::Customer(const std::string& id, const std::string& fullName, const PostalAddress& address,
                   const ContactDetails& contacts)
    : id_(detail::normalizeText(id)), fullName_(detail::normalizeText(fullName)), address_(address),
      contacts_(contacts) {
    if (!detail::hasPrintableText(id_) || id_.find(' ') != std::string::npos ||
        !detail::hasPrintableText(fullName_)) {
        throw std::invalid_argument("Customer needs an identifier without spaces and a full name");
    }
    // Контейнеры и строки сами управляют памятью: особый деструктор и копирование не нужны.
}

const std::string& Customer::id() const noexcept {
    return id_;
}

const std::string& Customer::fullName() const noexcept {
    return fullName_;
}

const PostalAddress& Customer::address() const noexcept {
    return address_;
}

const ContactDetails& Customer::contacts() const noexcept {
    return contacts_;
}

} // namespace post
