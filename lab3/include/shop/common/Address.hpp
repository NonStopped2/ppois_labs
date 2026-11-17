#pragma once

#include <string>
#include <string_view>

namespace shop {

/** Address.
 * Хранит проверенный адрес как значение. Изменение адресной книги не меняет адрес в
 * оформленном заказе.
 */
class Address {
  public:
    Address(std::string_view country, std::string_view postalCode, std::string_view city,
            std::string_view street, std::string_view building, std::string_view apartment = {});
    const std::string& country() const noexcept;
    const std::string& postalCode() const noexcept;
    const std::string& city() const noexcept;
    const std::string& street() const noexcept;
    const std::string& building() const noexcept;
    const std::string& apartment() const noexcept;
    bool operator==(const Address&) const = default;

  private:
    std::string country_;
    std::string postalCode_;
    std::string city_;
    std::string street_;
    std::string building_;
    std::string apartment_;
};

} // namespace shop
