#include "post/shipments/ContentItem.hpp"

#include "post/detail/Text.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"

#include <limits>
#include <stdexcept>

namespace post {

// Создаёт допустимую позицию описи; итоговые масса и ценность рассчитываются по запросу.
ContentItem::ContentItem(const std::string& name, std::int64_t quantity,
                         std::int64_t unitWeightGrams, Money unitDeclaredValue,
                         ContentCategory category)
    : name_(detail::normalizeText(name)), quantity_(quantity), unitWeightGrams_(unitWeightGrams),
      unitDeclaredValue_(unitDeclaredValue), category_(category) {
    if (!detail::hasPrintableText(name_) || quantity_ <= 0 || unitWeightGrams_ <= 0) {
        throw InvalidShipmentDataException("Contents require a name, positive quantity and weight");
    }
    if (category_ != ContentCategory::Documents && category_ != ContentCategory::GeneralGoods &&
        category_ != ContentCategory::FragileGoods) {
        throw InvalidShipmentDataException("Prohibited or unknown content category");
    }
}

// Нормализованное название для отображения позиции в описи содержимого.
const std::string& ContentItem::name() const noexcept {
    return name_;
}

// Число одинаковых единиц позиции; конструктор гарантирует положительное значение.
std::int64_t ContentItem::quantity() const noexcept {
    return quantity_;
}

// Масса одной единицы, которую нельзя путать с полной массой всей позиции.
std::int64_t ContentItem::unitWeightGrams() const noexcept {
    return unitWeightGrams_;
}

// Ценность одной единицы: читается без возможности изменить её через эту ссылку.
const Money& ContentItem::unitDeclaredValue() const noexcept {
    return unitDeclaredValue_;
}

// Категория для правил содержимого и упаковки, например обработки хрупких предметов.
ContentCategory ContentItem::category() const noexcept {
    return category_;
}

// Рассчитывает массу всей позиции, не допуская переполнения при большом количестве.
std::int64_t ContentItem::totalWeightGrams() const {
    // Деление до умножения исключает переполнение знакового целого (UB).
    if (quantity_ > std::numeric_limits<std::int64_t>::max() / unitWeightGrams_) {
        throw std::overflow_error("Total content weight is too large");
    }
    return quantity_ * unitWeightGrams_;
}

// Переиспользует арифметику Money вместо дублирования денежных проверок.
Money ContentItem::totalDeclaredValue() const {
    return unitDeclaredValue_.multiply(quantity_);
}

} // namespace post
