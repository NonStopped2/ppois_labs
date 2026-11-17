#include "shop/common/Address.hpp"
#include "shop/common/Validation.hpp"

namespace shop {

Address::Address(std::string_view country, std::string_view postalCode, std::string_view city,
                 std::string_view street, std::string_view building, std::string_view apartment)
    : country_(detail::text(country, "страна")), postalCode_(detail::text(postalCode, "индекс")),
      city_(detail::text(city, "город")), street_(detail::text(street, "улица")),
      building_(detail::text(building, "дом")),
      apartment_(detail::text(apartment, "квартира", true)) {
    detail::require(country_ == "BY", "Поддерживаются только адреса Беларуси (BY)");
    detail::require(postalCode_.size() == 6 && detail::digits(postalCode_),
                    "Почтовый индекс должен содержать шесть цифр");
}

const std::string& Address::country() const noexcept {
    return country_;
}
const std::string& Address::postalCode() const noexcept {
    return postalCode_;
}
const std::string& Address::city() const noexcept {
    return city_;
}
const std::string& Address::street() const noexcept {
    return street_;
}
const std::string& Address::building() const noexcept {
    return building_;
}
const std::string& Address::apartment() const noexcept {
    return apartment_;
}

} // namespace shop
