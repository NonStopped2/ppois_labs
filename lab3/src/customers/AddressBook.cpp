#include "shop/customers/AddressBook.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"

namespace shop {

AddressBook::AddressBook(std::string_view customerId)
    : customerId_(detail::text(customerId, "покупатель")) {}
const std::string& AddressBook::customerId() const noexcept {
    return customerId_;
}
const std::map<std::string, Address>& AddressBook::addresses() const noexcept {
    return addresses_;
}
const std::optional<std::string>& AddressBook::defaultAddressId() const noexcept {
    return defaultAddressId_;
}
Address AddressBook::address(std::string_view label) const {
    return detail::get(addresses_, label);
}

void AddressBook::add(std::string_view label, const Address& value) {
    const auto key = detail::text(label, "метка адреса");
    detail::check<DuplicateEntityException>(!addresses_.contains(key), "Метка адреса уже занята");
    auto selected = defaultAddressId_ ? defaultAddressId_ : std::optional<std::string>(key);
    addresses_.emplace(key, value);
    defaultAddressId_.swap(selected);
}
void AddressBook::replace(std::string_view label, const Address& value) {
    detail::get(addresses_, label);
    auto candidate = addresses_;
    candidate.at(std::string(label)) = value;
    addresses_.swap(candidate);
}
void AddressBook::remove(std::string_view label) {
    detail::get(addresses_, label);
    auto candidate = addresses_;
    candidate.erase(std::string(label));
    auto selected = defaultAddressId_;
    if (selected == label) {
        selected =
            candidate.empty() ? std::nullopt : std::optional<std::string>(candidate.begin()->first);
    }
    addresses_.swap(candidate);
    defaultAddressId_.swap(selected);
}
void AddressBook::chooseDefault(std::string_view label) {
    detail::get(addresses_, label);
    std::optional<std::string> selected{label};
    defaultAddressId_.swap(selected);
}

} // namespace shop
