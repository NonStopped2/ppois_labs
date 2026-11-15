#include "post/core/Money.hpp"

#include "post/exceptions/InvalidAmountException.hpp"

#include <limits>

// Money — денежная сумма для тарифов, платежей и ценности содержимого.
// Хранит неотрицательные целые копейки BYN, чтобы не использовать неточный double.
// Арифметика возвращает новый объект и проверяет переполнение до вычисления.
namespace post {

// Создаёт сумму; отрицательное значение прерывает создание объекта исключением.
Money::Money(std::int64_t kopecks) : kopecks_(kopecks) {
    if (kopecks < 0) {
        throw InvalidAmountException("Money cannot be negative");
    }
}

// Даёт копию суммы для отображения и расчётов, не открывая поле для изменения.
std::int64_t Money::kopecks() const noexcept {
    return kopecks_;
}

// Возвращает новую сумму сложения, не изменяя ни один из исходных объектов.
Money Money::add(const Money& other) const {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    // Сравниваем с остатком диапазона ДО сложения: само переполнение было бы UB.
    if (other.kopecks_ > maximum - kopecks_) {
        throw InvalidAmountException("Money addition overflow");
    }
    return Money(kopecks_ + other.kopecks_);
}

// Возвращает остаток; нехватка суммы — ошибка, а не разрешённый отрицательный баланс.
Money Money::subtract(const Money& other) const {
    if (other.kopecks_ > kopecks_) {
        throw InvalidAmountException("Money subtraction would be negative");
    }
    return Money(kopecks_ - other.kopecks_);
}

// Рассчитывает стоимость нескольких единиц; нулевой множитель даёт нулевую сумму.
Money Money::multiply(std::int64_t quantity) const {
    if (quantity < 0) {
        throw InvalidAmountException("Money multiplier cannot be negative");
    }
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    // Короткое замыкание && при нуле не допускает деления на ноль в правой части.
    if (quantity != 0 && kopecks_ > maximum / quantity) {
        throw InvalidAmountException("Money multiplication overflow");
    }
    return Money(kopecks_ * quantity);
}

} // namespace post
