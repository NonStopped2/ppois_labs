#include "post/sorting/MailBag.hpp"
#include "post/branches/PostalBranch.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"

// MailBag — мешок одного направления: хранит трек-номера, снимки массы и данные пломбы.
// Проверяет направление по SortingPlan и вместимость; не владеет самими отправлениями.
// Перед пломбированием сверяет груз с реестром, после — запрещает менять состав.
namespace post {
// Назначение и положительная вместимость определяются до добавления отправлений.
MailBag::MailBag(const std::string& id, std::int64_t maxWeightGrams,
                 const std::string& destinationBranchId)
    : id_(detail::identifier(id)), maxWeightGrams_(maxWeightGrams),
      destinationBranchId_(detail::identifier(destinationBranchId)) {
    if (maxWeightGrams_ <= 0) {
        throw std::invalid_argument("Bag capacity must be positive");
    }
}
// Ключ мешка уникален внутри партии.
const std::string& MailBag::id() const noexcept {
    return id_;
}
// Вместимость ограничивает суммарную массу, не одно отправление.
std::int64_t MailBag::maxWeightGrams() const noexcept {
    return maxWeightGrams_;
}
// Направление может быть промежуточным отделением маршрута.
const std::string& MailBag::destinationBranchId() const noexcept {
    return destinationBranchId_;
}
// Дата появляется вместе с успешно установленной пломбой.
std::optional<std::chrono::sys_seconds> MailBag::sealedAt() const noexcept {
    return sealedAt_;
}
// Пустая пломба означает открытый мешок.
const std::string& MailBag::sealId() const noexcept {
    return sealId_;
}
// Массы сохраняются рядом с номерами для проверяемого извлечения без второго реестра.
const std::map<std::string, std::int64_t>& MailBag::shipmentIds() const noexcept {
    return shipmentIds_;
}
// Накопленная масса меняется только после успешного изменения состава.
std::int64_t MailBag::totalWeightGrams() const noexcept {
    return totalWeightGrams_;
}
// Направление доказывает план сортировки, а не совпадение с конечным индексом клиента.
void MailBag::add(const Shipment& shipment, const PostalBranch& destination,
                  const SortingPlan& plan) {
    if (sealedAt_) {
        throw InvalidShipmentStateException("Bag is sealed");
    }
    if (destination.id() != destinationBranchId_ ||
        !plan.groupForDispatch({std::cref(shipment)}).contains(destinationBranchId_) ||
        shipmentIds_.contains(shipment.trackingNumber())) {
        throw InvalidShipmentStateException("Wrong bag direction or duplicate cargo");
    }
    if (shipment.weightGrams() > maxWeightGrams_ - totalWeightGrams_) {
        throw ShipmentLimitExceededException("Bag overweight");
    }
    shipmentIds_.emplace(shipment.trackingNumber(), shipment.weightGrams());
    totalWeightGrams_ += shipment.weightGrams();
}
// Вычитает именно сохранённую массу; не принимает подменённый внешний снимок отправления.
void MailBag::remove(const std::string& trackingNumber) {
    if (sealedAt_) {
        throw InvalidShipmentStateException("Bag is sealed");
    }
    const auto found = shipmentIds_.find(detail::identifier(trackingNumber));
    if (found == shipmentIds_.end()) {
        throw ShipmentNotFoundException("Cargo absent from bag");
    }
    totalWeightGrams_ -= found->second;
    shipmentIds_.erase(found);
}
// Сверяет снимок с реестром и хронологией; Returning сам по себе не доказывает наличия груза.
// Все элементы проверяются до установки пломбы, чтобы отказ не изменял мешок частично.
void MailBag::seal(const std::string& requestedSealId, std::chrono::sys_seconds time,
                   const ShipmentRegistry& registry) {
    detail::requireTime(time);
    auto seal = detail::identifier(requestedSealId);
    if (sealedAt_ || shipmentIds_.empty()) {
        throw InvalidShipmentStateException("Cannot seal this bag");
    }
    for (const auto& [cargoId, weight] : shipmentIds_) {
        const auto& shipment = registry.at(cargoId);
        if (!detail::availableForBranchStorage(shipment) || shipment.weightGrams() != weight ||
            shipment.history().events().back().occurredAt() > time) {
            throw InvalidShipmentStateException("Cargo changed or seal predates its history");
        }
    }
    sealId_.swap(seal);
    sealedAt_ = time;
}
} // namespace post
