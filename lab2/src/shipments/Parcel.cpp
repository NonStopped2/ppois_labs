#include "post/shipments/Parcel.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"

#include <algorithm>
#include <limits>
#include <utility>

// Parcel — вид Shipment для посылки: габариты, упаковка, опись ContentItem и пломба.
// Проверяет согласованность содержимого с массой, ценностью и условиями отправления.
// После пломбирования нельзя добавлять позиции; clone() сохраняет полный тип Parcel.
namespace post {
namespace {
// Суммирует массу или количество до заданного предела, не вычисляя опасную сумму.
template <typename Measure>
void requireTotal(const std::vector<ContentItem>& items, std::int64_t limit, Measure measure) {
    std::int64_t total = 0;
    for (const auto& item : items) {
        const auto value = measure(item);
        if (value > limit - total) {
            throw ShipmentLimitExceededException("Parcel contents exceed limit");
        }
        total += value;
    }
}
// Проверяет смысл хрупкой категории, а не только допустимость самого enum.
void validateFragile(const std::vector<ContentItem>& items, PackagingKind packaging, bool fragile) {
    const bool hasFragile = std::any_of(items.begin(), items.end(), [](const auto& item) {
        return item.category() == ContentCategory::FragileGoods;
    });
    if (hasFragile && (packaging != PackagingKind::Box || !fragile)) {
        throw InvalidShipmentDataException("Fragile contents require a box and fragile handling");
    }
}
} // namespace

// Создаёт открытую упаковку; содержимое и пломба добавляются отдельными операциями.
Parcel::Parcel(const Customer& sender, const Customer& recipient, std::int64_t weightGrams,
               const DeliveryOptions& options, const Dimensions& dimensions,
               PackagingKind packagingKind)
    : Shipment(sender, recipient, weightGrams, options), dimensions_(dimensions),
      packagingKind_(packagingKind) {
    options.validateCombination(ShipmentKind::Parcel);
    if (packagingKind_ != PackagingKind::Box && packagingKind_ != PackagingKind::PaddedEnvelope) {
        throw InvalidShipmentDataException("Unsupported parcel packaging");
    }
}
// Продукт определяет допустимые услуги.
ShipmentKind Parcel::kind() const noexcept {
    return ShipmentKind::Parcel;
}
// Габариты упаковки доступны для сравнения с лимитами, но не для внешнего изменения.
const Dimensions& Parcel::dimensions() const noexcept {
    return dimensions_;
}
// Только чтение защищает опись от обхода запрета изменения после пломбы.
const std::vector<ContentItem>& Parcel::contents() const noexcept {
    return contents_;
}
// Вид упаковки участвует в проверке хрупкого содержимого.
PackagingKind Parcel::packagingKind() const noexcept {
    return packagingKind_;
}
// Пустая строка означает ещё не запечатанную упаковку.
const std::string& Parcel::sealId() const noexcept {
    return sealId_;
}
// Глубокое копирование стандартных значений сохраняет отдельную опись.
std::unique_ptr<Shipment> Parcel::clone() const {
    return std::make_unique<Parcel>(*this);
}

// Добавляет позицию только в открытый черновик; масса всей описи не превышает полную массу.
void Parcel::addItem(const ContentItem& item) {
    requireDraft();
    requireRevisionRoom();
    if (!sealId_.empty()) {
        throw InvalidShipmentStateException("Parcel is sealed");
    }
    auto staged = contents_;
    staged.push_back(item);
    requireTotal(staged, weightGrams(), [](const auto& entry) { return entry.totalWeightGrams(); });
    validateFragile(staged, packagingKind_, options().fragile());
    contents_.swap(staged);
    incrementRevision();
}

// Пломбирует непустую посылку; идентификатор готовится до изменения состояния.
void Parcel::seal(const std::string& newSealId) {
    requireDraft();
    requireRevisionRoom();
    if (!sealId_.empty() || contents_.empty()) {
        throw InvalidShipmentStateException("Cannot seal this parcel");
    }
    auto prepared = detail::identifier(newSealId);
    sealId_.swap(prepared);
    incrementRevision();
}

// Проверяет готовность к приёму, количество единиц, упаковку и согласованность массы.
void Parcel::validateForPosting(std::int64_t maxWeightGrams, const Dimensions& sizeLimit,
                                std::int64_t maxUnits) const {
    validateCommonLimits(maxWeightGrams, sizeLimit, maxUnits);
    if (contents_.empty() || sealId_.empty()) {
        throw InvalidShipmentStateException("Parcel must contain items and be sealed");
    }
    requireTotal(contents_, maxUnits, [](const auto& item) { return item.quantity(); });
    requireTotal(contents_, weightGrams(),
                 [](const auto& item) { return item.totalWeightGrams(); });
    validateFragile(contents_, packagingKind_, options().fragile());
}

} // namespace post
