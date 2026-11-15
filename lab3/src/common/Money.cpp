#include "shop/common/Money.hpp"
#include "shop/common/Validation.hpp"

#include <limits>

namespace shop {
namespace {
constexpr auto Minimum = std::numeric_limits<std::int64_t>::min();
constexpr auto Maximum = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t BasisPointScale = 10000;

// Long division keeps intermediate products representable even near INT64_MAX.
std::uint64_t fraction(std::uint64_t value, std::uint64_t numerator, std::uint64_t divisor) {
    std::uint64_t quotient = 0;
    std::uint64_t remainder = 0;
    for (int bit = 62; bit >= 0; --bit) {
        quotient *= 2;
        remainder *= 2;
        if (remainder >= divisor) {
            remainder -= divisor;
            ++quotient;
        }
        if ((value >> bit) & 1U) {
            remainder += numerator;
            if (remainder >= divisor) {
                remainder -= divisor;
                ++quotient;
            }
        }
    }
    return quotient;
}
} // namespace

Money::Money(std::int64_t kopecks) noexcept : kopecks_(kopecks) {}

std::int64_t Money::kopecks() const noexcept {
    return kopecks_;
}

Money Money::checkedAdd(Money other) const {
    const auto right = other.kopecks_;
    detail::require(
        !((right > 0 && kopecks_ > Maximum - right) || (right < 0 && kopecks_ < Minimum - right)),
        "Переполнение денежной суммы");
    return Money(kopecks_ + right);
}

Money Money::checkedSubtract(Money other) const {
    const auto right = other.kopecks_;
    detail::require(
        !((right < 0 && kopecks_ > Maximum + right) || (right > 0 && kopecks_ < Minimum + right)),
        "Переполнение денежной разности");
    return Money(kopecks_ - right);
}

Money Money::multiply(std::int64_t quantity) const {
    detail::require(quantity >= 0, "Количество не может быть отрицательным");
    if (quantity == 0) {
        return Money{};
    }
    detail::require(kopecks_ <= Maximum / quantity && kopecks_ >= Minimum / quantity,
                    "Переполнение стоимости количества");
    return Money(kopecks_ * quantity);
}

Money Money::fractionFloor(std::int64_t numerator, std::int64_t denominator) const {
    detail::require(kopecks_ >= 0 && denominator > 0 && numerator >= 0 && numerator <= denominator,
                    "Некорректная доля денежной суммы");
    return Money(static_cast<std::int64_t>(fraction(kopecks_, numerator, denominator)));
}

Money Money::percentageFloor(std::int64_t basisPoints) const {
    return fractionFloor(basisPoints, BasisPointScale);
}

std::string Money::format() const {
    // Unsigned arithmetic handles the magnitude of INT64_MIN without negating it.
    const auto magnitude = kopecks_ < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(kopecks_)
                                        : static_cast<std::uint64_t>(kopecks_);
    const auto cents = magnitude % 100;
    return (kopecks_ < 0 ? "-" : "") + std::to_string(magnitude / 100) + "." +
           (cents < 10 ? "0" : "") + std::to_string(cents) + " BYN";
}

} // namespace shop
