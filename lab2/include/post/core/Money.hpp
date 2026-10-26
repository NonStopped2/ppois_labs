#pragma once

// Тип результата сгенерированного operator<=> требует этот заголовок.
#include <compare> // IWYU pragma: keep
#include <cstdint>

namespace post {

// Денежная сумма для стоимости содержимого и почтовых услуг.
// Хранит неотрицательные копейки BYN и проверяет арифметику без округления double.
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
