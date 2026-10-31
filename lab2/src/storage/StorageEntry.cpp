#include "post/storage/StorageEntry.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include "post/detail/Validation.hpp"
#include <algorithm>

// StorageEntry — запись размещения в ячейке: трек-номер, масса, размеры, приём и дедлайн.
// Не владеет Shipment, а сохраняет снимок данных для учёта вместимости и срока хранения.
// Создаётся только для доступного к хранению груза; по StoragePolicy считает плату.
namespace post {
namespace {
// В пути, у курьера и после завершения объект не должен занимать ячейку отделения.
void requireStorable(const Shipment& shipment, std::chrono::sys_seconds time) {
    if (shipment.trackingNumber().empty() || !detail::availableForBranchStorage(shipment)) {
        throw InvalidShipmentStateException("Shipment is not available for branch storage");
    }
    if (!shipment.history().events().empty() &&
        time < shipment.history().events().back().occurredAt()) {
        throw InvalidShipmentStateException("Storage precedes latest shipment event");
    }
}
} // namespace
// Копирует физические параметры и вычисляет крайний срок до изменения какой-либо ячейки.
StorageEntry::StorageEntry(const Shipment& shipment, const StoragePolicy& policy,
                           std::chrono::sys_seconds receivedAt, const std::string& cellId)
    : shipmentId_(shipment.trackingNumber()), storedWeightGrams_(shipment.weightGrams()),
      dimensions_(shipment.dimensions()), receivedAt_(receivedAt),
      deadlineAt_(policy.deadlineFrom(receivedAt)), cellId_(detail::identifier(cellId)) {
    requireStorable(shipment, receivedAt_);
}
// Номер связывает запись с единственным авторитетным объектом реестра.
const std::string& StorageEntry::shipmentId() const noexcept {
    return shipmentId_;
}
// Масса остаётся снимком, а не вычисляется через внешнюю изменяемую ссылку.
std::int64_t StorageEntry::storedWeightGrams() const noexcept {
    return storedWeightGrams_;
}
// Габариты нужны для проверки вместимости.
const Dimensions& StorageEntry::dimensions() const noexcept {
    return dimensions_;
}
// Время поступления задаётся явно.
std::chrono::sys_seconds StorageEntry::receivedAt() const noexcept {
    return receivedAt_;
}
// Сохранённый дедлайн не продлевается из-за более позднего запроса.
std::chrono::sys_seconds StorageEntry::deadlineAt() const noexcept {
    return deadlineAt_;
}
// Идентификатор места переносится вместе со снимком при извлечении.
const std::string& StorageEntry::cellId() const noexcept {
    return cellId_;
}
// На точной границе срок уже истёк, как и в StoragePolicy.
bool StorageEntry::isOverdueAt(std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    if (time < receivedAt_) {
        throw std::invalid_argument("Query precedes storage");
    }
    return time >= deadlineAt_;
}
// Переданная политика определяет цену, сохранённый дедлайн дополнительно ограничивает начисление.
Money StorageEntry::storageFeeUntil(const StoragePolicy& policy,
                                    std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    if (time < receivedAt_) {
        throw std::invalid_argument("Query precedes storage");
    }
    return policy.chargeUntil(receivedAt_, std::min(time, deadlineAt_));
}
} // namespace post
