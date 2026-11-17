#pragma once

#include "shop/common/Address.hpp"
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace shop {

/** AddressBook.
 * Хранит именованные адреса покупателя. При удалении основного адреса выбирает первый
 * оставшийся.
 */
class AddressBook {
  public:
    explicit AddressBook(std::string_view customerId);
    const std::string& customerId() const noexcept;
    const std::map<std::string, Address>& addresses() const noexcept;
    const std::optional<std::string>& defaultAddressId() const noexcept;
    Address address(std::string_view label) const;
    void add(std::string_view label, const Address& address);
    void replace(std::string_view label, const Address& address);
    void remove(std::string_view label);
    void chooseDefault(std::string_view label);
    bool operator==(const AddressBook&) const = default;

  private:
    std::string customerId_;
    std::map<std::string, Address> addresses_;
    std::optional<std::string> defaultAddressId_;
};

} // namespace shop
