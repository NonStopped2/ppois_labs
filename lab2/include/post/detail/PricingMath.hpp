#pragma once
#include "post/core/Money.hpp"
#include "post/exceptions/InvalidAmountException.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>

namespace post::detail {
inline constexpr std::int64_t kBasisPointScale = 10000;

// Без заданного предела переполнение является ошибкой, с пределом — насыщением до него.
inline std::int64_t boundedAdd(std::int64_t first, std::int64_t second, std::optional<Money> cap) {
    const auto limit = cap ? cap->kopecks() : std::numeric_limits<std::int64_t>::max();
    if (second > limit - first) {
        if (cap) {
            return limit;
        }
        throw InvalidAmountException("Price addition overflow");
    }
    return first + second;
}
// Проверяет произведение до вычисления; нулевой множитель не вызывает деления на ноль.
inline std::int64_t boundedProduct(std::int64_t first, std::int64_t second,
                                   std::optional<Money> cap) {
    if (second == 0) {
        return 0;
    }
    const auto limit = cap ? cap->kopecks() : std::numeric_limits<std::int64_t>::max();
    if (first > limit / second) {
        if (cap) {
            return limit;
        }
        throw InvalidAmountException("Price multiplication overflow");
    }
    return first * second;
}
// Округляет вверх до шага копеек; явный предел имеет приоритет над кратностью шагу.
inline Money roundCharge(Money amount, std::int64_t step, std::optional<Money> cap = std::nullopt) {
    if (step <= 0) {
        throw std::invalid_argument("Rounding step must be positive");
    }
    const auto raw = cap ? std::min(amount.kopecks(), cap->kopecks()) : amount.kopecks();
    const auto remainder = raw % step;
    return Money(remainder == 0 ? raw : boundedAdd(raw, step - remainder, cap));
}
// Делит на масштаб до опасных умножений: (aD+b)(cD+d)/D = a*factor + b*c + b*d/D.
inline Money scaledCharge(Money basis, std::int64_t factor, std::int64_t step = 1,
                          std::optional<Money> cap = std::nullopt) {
    if (factor < 0) {
        throw std::invalid_argument("Negative price factor");
    }
    const auto whole = boundedProduct(basis.kopecks() / kBasisPointScale, factor, cap);
    const auto remainder = basis.kopecks() % kBasisPointScale;
    const auto other = boundedProduct(remainder, factor / kBasisPointScale, cap);
    const auto smallProduct = remainder * (factor % kBasisPointScale);
    const auto fraction = smallProduct / kBasisPointScale + (smallProduct % kBasisPointScale != 0);
    const auto subtotal = boundedAdd(whole, other, cap);
    return roundCharge(Money(boundedAdd(subtotal, fraction, cap)), step, cap);
}

} // namespace post::detail
