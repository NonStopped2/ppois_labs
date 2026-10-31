#include "post/core/Dimensions.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

// Dimensions — габариты конверта, посылки или предела складской ячейки.
// Хранит три положительные стороны по убыванию: перестановка сторон не меняет размер.
// Проверяет вхождение в лимит и безопасно считает объём и длину с обхватом.
namespace post {
namespace {

constexpr std::int64_t kGirthMultiplier = 2;

// Общее безопасное умножение для промежуточных геометрических расчётов.
std::int64_t checkedProduct(std::int64_t first, std::int64_t second) {
    // Здесь оба множителя положительны благодаря инварианту Dimensions.
    if (first > std::numeric_limits<std::int64_t>::max() / second) {
        throw std::overflow_error("Dimensions multiplication overflow");
    }
    return first * second;
}

// Проверяет сумму до сложения, чтобы промежуточный результат тоже не переполнился.
std::int64_t checkedSum(std::int64_t first, std::int64_t second) {
    if (first > std::numeric_limits<std::int64_t>::max() - second) {
        throw std::overflow_error("Dimensions addition overflow");
    }
    return first + second;
}

} // namespace

// Создаёт положительные габариты в единой ориентации независимо от порядка аргументов.
Dimensions::Dimensions(std::int64_t lengthMm, std::int64_t widthMm, std::int64_t heightMm)
    : lengthMm_(lengthMm), widthMm_(widthMm), heightMm_(heightMm) {
    if (lengthMm <= 0 || widthMm <= 0 || heightMm <= 0) {
        throw std::invalid_argument("Every dimension must be positive");
    }
    // sort упорядочивает по возрастанию; сохраняем наоборот: length >= width >= height.
    std::array sides{lengthMm, widthMm, heightMm};
    std::sort(sides.begin(), sides.end());
    lengthMm_ = sides[2];
    widthMm_ = sides[1];
    heightMm_ = sides[0];
}

// Наибольшая сторона в миллиметрах, а не обязательно первый аргумент конструктора.
std::int64_t Dimensions::lengthMm() const noexcept {
    return lengthMm_;
}

// Средняя сторона канонической тройки для проверки соответствующего ограничения.
std::int64_t Dimensions::widthMm() const noexcept {
    return widthMm_;
}

// Наименьшая сторона; у конверта именно она соответствует толщине.
std::int64_t Dimensions::heightMm() const noexcept {
    return heightMm_;
}

// Проверяет вхождение с разрешённым поворотом упаковки; равенство пределу допустимо.
bool Dimensions::fitsWithin(const Dimensions& limit) const noexcept {
    // После сортировки достаточно сравнить соответствующие стороны.
    return lengthMm_ <= limit.lengthMm_ && widthMm_ <= limit.widthMm_ &&
           heightMm_ <= limit.heightMm_;
}

// Вычисляет объём, проверяя оба умножения, а не только окончательный результат.
std::int64_t Dimensions::volumeCubicMm() const {
    const auto baseArea = checkedProduct(lengthMm_, widthMm_);
    return checkedProduct(baseArea, heightMm_);
}

// Почтовый размер: наибольшая сторона + 2 * сумма двух остальных сторон.
std::int64_t Dimensions::lengthPlusGirthMm() const {
    const auto crossSectionSum = checkedSum(widthMm_, heightMm_);
    const auto girth = checkedProduct(crossSectionSum, kGirthMultiplier);
    return checkedSum(lengthMm_, girth);
}

} // namespace post
