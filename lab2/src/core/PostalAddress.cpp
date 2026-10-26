#include "post/core/PostalAddress.hpp"

#include "post/detail/Text.hpp"
#include "post/exceptions/InvalidAddressException.hpp"

#include <algorithm>
#include <utility>

namespace post {

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

const std::string& PostalAddress::postalCode() const noexcept {
    return postalCode_;
}

const std::string& PostalAddress::locality() const noexcept {
    return locality_;
}

const std::string& PostalAddress::street() const noexcept {
    return street_;
}

const std::string& PostalAddress::building() const noexcept {
    return building_;
}

const std::optional<std::string>& PostalAddress::apartment() const noexcept {
    return apartment_;
}

void PostalAddress::validate() const {
    constexpr std::size_t postalCodeLength = 6;
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

bool PostalAddress::sameDeliveryPointAs(const PostalAddress& other) const noexcept {
    return *this == other;
}

} // namespace post
