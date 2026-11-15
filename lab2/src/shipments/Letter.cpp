#include "post/shipments/Letter.hpp"
#include "post/detail/Text.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"

// Letter — вид Shipment для письма: добавляет конверт, число листов и описание вложения.
// Проверяет общие ограничения массы и размеров и отдельный лимит количества листов.
// Через виртуальные методы даёт общим процессам размеры конверта и копию именно Letter.
namespace post {

// Непустое описание и положительное число листов обязательны ещё до регистрации.
Letter::Letter(const Customer& sender, const Customer& recipient, std::int64_t weightGrams,
               const DeliveryOptions& options, const Dimensions& envelope, std::int64_t sheetCount,
               const std::string& contentsDescription)
    : Shipment(sender, recipient, weightGrams, options), envelope_(envelope),
      sheetCount_(sheetCount), contentsDescription_(detail::normalizeText(contentsDescription)) {
    options.validateCombination(ShipmentKind::Letter);
    if (sheetCount_ <= 0 || !detail::hasPrintableText(contentsDescription_)) {
        throw InvalidShipmentDataException("Letter requires sheets and a description");
    }
}
// Вид продукта выбирает правила услуг и тарификации.
ShipmentKind Letter::kind() const noexcept {
    return ShipmentKind::Letter;
}
// Общий полиморфный доступ к размерам использует размеры конверта.
const Dimensions& Letter::dimensions() const noexcept {
    return envelope_;
}
// Конверт хранится независимо от переданного объекта Dimensions.
const Dimensions& Letter::envelope() const noexcept {
    return envelope_;
}
// Количество листов проверяется против отдельного лимита maxUnits.
std::int64_t Letter::sheetCount() const noexcept {
    return sheetCount_;
}
// Нормализованное описание нужно для оформления письма, а не для расчёта массы.
const std::string& Letter::contentsDescription() const noexcept {
    return contentsDescription_;
}
// Полиморфная копия сохраняет именно Letter, не обрезая данные до Shipment.
std::unique_ptr<Shipment> Letter::clone() const {
    return std::make_unique<Letter>(*this);
}
// Проверяет массу, ориентационно-независимые габариты и число листов.
void Letter::validateForPosting(std::int64_t maxWeightGrams, const Dimensions& sizeLimit,
                                std::int64_t maxUnits) const {
    validateCommonLimits(maxWeightGrams, sizeLimit, maxUnits);
    if (sheetCount_ > maxUnits) {
        throw ShipmentLimitExceededException("Too many letter sheets");
    }
}

} // namespace post
