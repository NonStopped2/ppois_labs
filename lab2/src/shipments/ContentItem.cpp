#include "post/shipments/ContentItem.hpp"

#include "post/detail/Text.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"

#include <limits>
#include <stdexcept>

namespace post {

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

const std::string& ContentItem::name() const noexcept {
    return name_;
}

std::int64_t ContentItem::quantity() const noexcept {
    return quantity_;
}

std::int64_t ContentItem::unitWeightGrams() const noexcept {
    return unitWeightGrams_;
}

const Money& ContentItem::unitDeclaredValue() const noexcept {
    return unitDeclaredValue_;
}

ContentCategory ContentItem::category() const noexcept {
    return category_;
}

std::int64_t ContentItem::totalWeightGrams() const {
    // Деление до умножения исключает переполнение знакового целого (UB).
    if (quantity_ > std::numeric_limits<std::int64_t>::max() / unitWeightGrams_) {
        throw std::overflow_error("Total content weight is too large");
    }
    return quantity_ * unitWeightGrams_;
}

Money ContentItem::totalDeclaredValue() const {
    return unitDeclaredValue_.multiply(quantity_);
}

} // namespace post
