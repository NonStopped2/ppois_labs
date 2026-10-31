#include "post/storage/StorageArea.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"
#include "post/exceptions/StorageCapacityException.hpp"
#include <algorithm>
#include <set>

// StorageArea — зона склада: собственные ячейки и правила совместимости с видами отправлений.
// Выбирает первую подходящую ячейку и размещает/извлекает записи через StorageCell.
// План группы строится на копии ячеек, чтобы учесть общий груз без изменения живого склада.
namespace post {
namespace {
// Пробуем ячейки в стабильном порядке, перехватывая только отказ по вместимости.
std::string placeFirst(std::vector<StorageCell>& cells, const Shipment& shipment,
                       const StoragePolicy& policy, std::chrono::sys_seconds time) {
    for (auto& cell : cells) {
        try {
            auto id = cell.id();
            cell.place(shipment, policy, time);
            return id;
        } catch (const StorageCapacityException&) {
            continue;
        }
    }
    throw StorageCapacityException("No suitable cell in storage area");
}
// Проверяет ячейки как конфигурацию новой пустой зоны, а не как импорт занятого склада.
void validateCells(const std::vector<StorageCell>& cells) {
    std::set<std::string> ids;
    std::for_each(cells.begin(), cells.end(), [&ids](const auto& cell) {
        if (!cell.entries().empty() || !ids.insert(cell.id()).second) {
            throw std::invalid_argument("Area requires unique empty cells");
        }
    });
}
// Проверяет совместимость справочника продуктов с назначением зоны.
void validateKinds(const std::vector<ShipmentKind>& values, StorageAreaKind areaKind) {
    std::set<ShipmentKind> kinds;
    std::for_each(values.begin(), values.end(), [&kinds, areaKind](const auto value) {
        if ((value != ShipmentKind::Letter && value != ShipmentKind::Parcel) ||
            !kinds.insert(value).second ||
            (areaKind == StorageAreaKind::Fragile && value != ShipmentKind::Parcel)) {
            throw std::invalid_argument("Invalid area product kinds");
        }
    });
}
} // namespace
// Уникальные ячейки и известные виды проверяются до использования зоны.
StorageArea::StorageArea(const std::string& id, const std::string& name, StorageAreaKind kind,
                         std::vector<StorageCell> cells,
                         std::vector<ShipmentKind> allowedShipmentKinds)
    : id_(detail::identifier(id)), name_(detail::normalizeText(name)), kind_(kind),
      cells_(std::move(cells)), allowedShipmentKinds_(std::move(allowedShipmentKinds)) {
    if (!detail::hasPrintableText(name_) || cells_.empty() || allowedShipmentKinds_.empty() ||
        (kind_ != StorageAreaKind::General && kind_ != StorageAreaKind::Fragile)) {
        throw std::invalid_argument("Invalid storage area");
    }
    validateCells(cells_);
    validateKinds(allowedShipmentKinds_, kind_);
}
// Идентификатор отличает зону внутри отделения.
const std::string& StorageArea::id() const noexcept {
    return id_;
}
// Имя нормализуется, но не используется как ключ.
const std::string& StorageArea::name() const noexcept {
    return name_;
}
// Хрупкая зона выделяется явно, а не по тексту имени.
StorageAreaKind StorageArea::kind() const noexcept {
    return kind_;
}
// Только чтение не позволяет менять идентификаторы ячеек извне.
const std::vector<StorageCell>& StorageArea::cells() const noexcept {
    return cells_;
}
// Допустимые продукты задаются конфигурацией.
const std::vector<ShipmentKind>& StorageArea::allowedShipmentKinds() const noexcept {
    return allowedShipmentKinds_;
}
// Наличие проверяется по всем ячейкам зоны.
bool StorageArea::contains(const std::string& shipmentId) const {
    return std::any_of(cells_.begin(), cells_.end(),
                       [&shipmentId](const auto& cell) { return cell.contains(shipmentId); });
}
// General не принимает хрупкие отправления; Fragile предназначена именно для них.
void StorageArea::requireCompatible(const Shipment& shipment) const {
    const bool allowed = std::find(allowedShipmentKinds_.begin(), allowedShipmentKinds_.end(),
                                   shipment.kind()) != allowedShipmentKinds_.end();
    if (!allowed || shipment.options().fragile() != (kind_ == StorageAreaKind::Fragile)) {
        throw StorageCapacityException("Shipment incompatible with storage area");
    }
}
// Все пробные вставки происходят в копиях; неуспех не оставляет частичного размещения.
std::map<std::string, std::string>
StorageArea::planPlacement(const std::vector<std::reference_wrapper<const Shipment>>& shipments,
                           const StoragePolicy& policy, std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    auto staged = cells_;
    std::map<std::string, std::string> result;
    for (const auto reference : shipments) {
        const auto& shipment = reference.get();
        requireCompatible(shipment);
        if (contains(shipment.trackingNumber()) || result.contains(shipment.trackingNumber())) {
            throw StorageCapacityException("Duplicate placement");
        }
        result.emplace(shipment.trackingNumber(), placeFirst(staged, shipment, policy, time));
    }
    return result;
}
// Вид берётся из авторитетного реестра: в записи размещения отдельного изменяемого вида нет.
std::map<ShipmentKind, std::size_t>
StorageArea::occupancyByKind(const ShipmentRegistry& registry) const {
    std::map<ShipmentKind, std::size_t> result;
    for (const auto value : allowedShipmentKinds_) {
        result[value] = 0;
    }
    for (const auto& cell : cells_) {
        for (const auto& entry : cell.entries()) {
            ++result[registry.at(entry.shipmentId()).kind()];
        }
    }
    return result;
}
// Делегирует ту же проверку совместимости, но сохраняет размещение в собственных ячейках.
void StorageArea::place(const Shipment& shipment, const StoragePolicy& policy,
                        std::chrono::sys_seconds time) {
    requireCompatible(shipment);
    if (contains(shipment.trackingNumber())) {
        throw StorageCapacityException("Duplicate placement");
    }
    (void)placeFirst(cells_, shipment, policy, time);
}
// Возвращает исходный снимок; отсутствие записи не меняет зону.
StorageEntry StorageArea::take(const std::string& shipmentId) {
    const auto number = detail::identifier(shipmentId);
    const auto cell = std::find_if(cells_.begin(), cells_.end(),
                                   [&number](const auto& value) { return value.contains(number); });
    if (cell == cells_.end()) {
        throw ShipmentNotFoundException("Shipment absent from area");
    }
    return cell->take(number);
}
} // namespace post
