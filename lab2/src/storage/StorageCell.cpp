#include "post/storage/StorageCell.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"
#include "post/exceptions/StorageCapacityException.hpp"
#include <algorithm>
#include <numeric>

// StorageCell — складская ячейка с пределами массы, числа мест и размеров одного отправления.
// Владеет записями StorageEntry, не самими Shipment; place() проверяет лимиты до добавления.
// take() удаляет размещение и возвращает его снимок; используется внутри StorageArea.
namespace post {
// Положительные лимиты исключают неоднозначные «неограниченные» отрицательные значения.
StorageCell::StorageCell(const std::string& id, std::int64_t maxWeightGrams, std::int64_t maxItems,
                         const Dimensions& limit)
    : id_(detail::identifier(id)), maxWeightGrams_(maxWeightGrams), maxItems_(maxItems),
      limit_(limit) {
    if (maxWeightGrams_ <= 0 || maxItems_ <= 0) {
        throw std::invalid_argument("Cell limits must be positive");
    }
}
// Идентификаторы ячеек уникальны внутри зоны и проверяются её конструктором.
const std::string& StorageCell::id() const noexcept {
    return id_;
}
// Лимит относится к сумме масс всех записей.
std::int64_t StorageCell::maxWeightGrams() const noexcept {
    return maxWeightGrams_;
}
// Количество ограничивается отдельно от массы.
std::int64_t StorageCell::maxItems() const noexcept {
    return maxItems_;
}
// Каждое отправление должно помещаться по всем трём сторонам.
const Dimensions& StorageCell::limit() const noexcept {
    return limit_;
}
// Внешний код не может вписать запись в обход place.
const std::vector<StorageEntry>& StorageCell::entries() const noexcept {
    return entries_;
}
// Сумма безопасна: каждая вставка заранее проверяет оставшуюся вместимость.
std::int64_t StorageCell::totalWeightGrams() const {
    return std::accumulate(
        entries_.begin(), entries_.end(), std::int64_t{0},
        [](auto sum, const auto& entry) { return sum + entry.storedWeightGrams(); });
}
// Проверка наличия используется также при планировании без изменения ячейки.
bool StorageCell::contains(const std::string& shipmentId) const {
    return std::any_of(entries_.begin(), entries_.end(), [&shipmentId](const auto& entry) {
        return entry.shipmentId() == shipmentId;
    });
}
// Проверяет ограничения до создания записи и вставки, избегая переполнения суммы масс.
void StorageCell::place(const Shipment& shipment, const StoragePolicy& policy,
                        std::chrono::sys_seconds time) {
    if (contains(shipment.trackingNumber()) ||
        entries_.size() >= static_cast<std::uint64_t>(maxItems_) ||
        shipment.weightGrams() > maxWeightGrams_ - totalWeightGrams() ||
        !shipment.dimensions().fitsWithin(limit_)) {
        throw StorageCapacityException("Cell has duplicate shipment or insufficient capacity");
    }
    entries_.emplace_back(shipment, policy, time, id_);
}
// Сначала копирует результат: ошибка выделения памяти не должна удалить исходную запись.
StorageEntry StorageCell::take(const std::string& shipmentId) {
    const auto number = detail::identifier(shipmentId);
    const auto found = std::find_if(entries_.begin(), entries_.end(), [&number](const auto& entry) {
        return entry.shipmentId() == number;
    });
    if (found == entries_.end()) {
        throw ShipmentNotFoundException("Shipment absent from cell");
    }
    auto result = *found;
    entries_.erase(found);
    return result;
}
} // namespace post
