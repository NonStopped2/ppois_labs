#pragma once

// Тип результата сгенерированного operator<=> требует этот заголовок.
#include <compare> // IWYU pragma: keep
#include <cstdint>

namespace post {

// Money — денежная сумма для тарифов, платежей и ценности содержимого.
// Хранит неотрицательные целые копейки BYN, чтобы не использовать неточный double.
// Арифметика возвращает новый объект и проверяет переполнение до вычисления.
class Money {
public:
    explicit Money(std::int64_t kopecks = 0);

    [[nodiscard]] std::int64_t kopecks() const noexcept;
    [[nodiscard]] Money add(const Money& other) const;
    [[nodiscard]] Money subtract(const Money& other) const;
    [[nodiscard]] Money multiply(std::int64_t quantity) const;

    auto operator<=>(const Money& other) const = default;

private:
    std::int64_t kopecks_;
};

} // namespace post
