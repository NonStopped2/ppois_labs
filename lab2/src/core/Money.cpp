#include "post/core/Money.hpp"

#include "post/exceptions/InvalidAmountException.hpp"

#include <limits>

namespace post {

Money::Money(std::int64_t kopecks) : kopecks_(kopecks) {
    if (kopecks < 0) {
        throw InvalidAmountException("Money cannot be negative");
    }
}

std::int64_t Money::kopecks() const noexcept {
    return kopecks_;
}

Money Money::add(const Money& other) const {
    // Проверка выполняется ДО сложения: переполнение знакового int — undefined behavior.
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    if (other.kopecks_ > maximum - kopecks_) {
        throw InvalidAmountException("Money addition overflow");
    }
    return Money(kopecks_ + other.kopecks_);
}

Money Money::subtract(const Money& other) const {
    if (other.kopecks_ > kopecks_) {
        throw InvalidAmountException("Money subtraction would be negative");
    }
    return Money(kopecks_ - other.kopecks_);
}

Money Money::multiply(std::int64_t quantity) const {
    if (quantity < 0) {
        throw InvalidAmountException("Money multiplier cannot be negative");
    }
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    // Отдельная проверка нуля защищает само деление в проверке переполнения.
    if (quantity != 0 && kopecks_ > maximum / quantity) {
        throw InvalidAmountException("Money multiplication overflow");
    }
    return Money(kopecks_ * quantity);
}

} // namespace post
