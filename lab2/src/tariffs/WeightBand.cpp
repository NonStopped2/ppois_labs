#include "post/tariffs/WeightBand.hpp"
#include "post/exceptions/TariffNotFoundException.hpp"
#include <stdexcept>

namespace post {
// Нижняя граница исключена, верхняя включена; шаг обязан быть положительным.
WeightBand::WeightBand(std::int64_t lowerExclusiveGrams, std::int64_t upperInclusiveGrams,
                       Money basePrice, std::int64_t stepGrams, Money stepPrice)
    : lowerExclusiveGrams_(lowerExclusiveGrams), upperInclusiveGrams_(upperInclusiveGrams),
      basePrice_(basePrice), stepGrams_(stepGrams), stepPrice_(stepPrice) {
    if (lowerExclusiveGrams_ < 0 || upperInclusiveGrams_ <= lowerExclusiveGrams_ ||
        stepGrams_ <= 0) {
        throw std::invalid_argument("Invalid weight interval or step");
    }
}
// Исключённая нижняя граница позволяет соседним интервалам не пересекаться.
std::int64_t WeightBand::lowerExclusiveGrams() const noexcept {
    return lowerExclusiveGrams_;
}
// Включённая верхняя граница допускает точное достижение лимита.
std::int64_t WeightBand::upperInclusiveGrams() const noexcept {
    return upperInclusiveGrams_;
}
// Базовая цена покрывает первый начавшийся шаг внутри интервала.
const Money& WeightBand::basePrice() const noexcept {
    return basePrice_;
}
// Размер каждого оплачиваемого весового шага в граммах.
std::int64_t WeightBand::stepGrams() const noexcept {
    return stepGrams_;
}
// Цена каждого шага после первого; ноль означает плоский тариф внутри интервала.
const Money& WeightBand::stepPrice() const noexcept {
    return stepPrice_;
}
// Принадлежность проверяется без вычитания потенциально отрицательной массы.
bool WeightBand::contains(std::int64_t weightGrams) const noexcept {
    return weightGrams > lowerExclusiveGrams_ && weightGrams <= upperInclusiveGrams_;
}
// (delta - 1) / step равно числу дополнительных шагов и не требует сложения перед делением.
Money WeightBand::priceFor(std::int64_t weightGrams) const {
    if (!contains(weightGrams)) {
        throw TariffNotFoundException("Weight outside tariff band");
    }
    const auto extraSteps = (weightGrams - lowerExclusiveGrams_ - 1) / stepGrams_;
    return basePrice_.add(stepPrice_.multiply(extraSteps));
}
// Касание исключённой и включённой границ не является пересечением.
bool WeightBand::overlaps(const WeightBand& other) const noexcept {
    return lowerExclusiveGrams_ < other.upperInclusiveGrams_ &&
           other.lowerExclusiveGrams_ < upperInclusiveGrams_;
}

} // namespace post
