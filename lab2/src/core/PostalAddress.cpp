#include "post/core/PostalAddress.hpp"

#include "post/detail/Text.hpp"
#include "post/exceptions/InvalidAddressException.hpp"

#include <algorithm>
#include <utility>

// PostalAddress — адрес клиента для доставки, выбора тарифной зоны и сортировки.
// Хранит шестизначный индекс, населённый пункт, улицу, дом и необязательную квартиру.
// Проверяет формат данных, но не существование адреса; индекс — строка ради ведущих нулей.
namespace post {

// Нормализует пробелы и создаёт только адрес, прошедший проверку обязательных частей.
PostalAddress::PostalAddress(const std::string& postalCode, const std::string& locality,
                             const std::string& street, const std::string& building,
                             std::optional<std::string> apartment)
    : postalCode_(detail::normalizeText(postalCode)), locality_(detail::normalizeText(locality)),
      street_(detail::normalizeText(street)), building_(detail::normalizeText(building)),
      apartment_(std::move(apartment)) {
    if (apartment_) {
        apartment_ = detail::normalizeText(*apartment_);
    }
    validate();
}

// Индекс для маршрутизации: строковое представление сохраняет ведущие нули.
const std::string& PostalAddress::postalCode() const noexcept {
    return postalCode_;
}

// Нормализованный населённый пункт для адресной строки, без изменения регистра.
const std::string& PostalAddress::locality() const noexcept {
    return locality_;
}

// Название улицы уже без лишних пробелов; наружу доступно только для чтения.
const std::string& PostalAddress::street() const noexcept {
    return street_;
}

// Дом представлен строкой, поскольку его обозначение может содержать буквы и корпус.
const std::string& PostalAddress::building() const noexcept {
    return building_;
}

// optional отличает адрес без квартиры от адреса с указанным номером.
const std::optional<std::string>& PostalAddress::apartment() const noexcept {
    return apartment_;
}

// Проверяет структуру адреса, но не существование дома в реальном справочнике.
void PostalAddress::validate() const {
    constexpr std::size_t postalCodeLength = 6;
    // all_of требует ASCII-цифру на каждой позиции, а не просто подходящую длину.
    const bool correctIndex =
        postalCode_.size() == postalCodeLength &&
        std::all_of(postalCode_.begin(), postalCode_.end(), detail::isAsciiDigit);
    if (!correctIndex) {
        throw InvalidAddressException("Postal code must contain exactly six ASCII digits");
    }
    if (!detail::hasPrintableText(locality_) || !detail::hasPrintableText(street_) ||
        !detail::hasPrintableText(building_)) {
        throw InvalidAddressException("Locality, street and building must contain printable text");
    }
    // Отсутствие квартиры допустимо, но явно переданная пустая квартира — ошибка ввода.
    if (apartment_ && !detail::hasPrintableText(*apartment_)) {
        throw InvalidAddressException("An apartment must be omitted or contain printable text");
    }
}

// Сравнивает точку доставки по всем адресным полям; имя получателя сюда не входит.
bool PostalAddress::sameDeliveryPointAs(const PostalAddress& other) const noexcept {
    // *this — текущий адрес; default operator== сравнивает все его поля.
    return *this == other;
}

} // namespace post
