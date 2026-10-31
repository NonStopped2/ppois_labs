#pragma once
#include "post/registry/ShipmentRegistry.hpp"
#include "post/storage/StorageAreaKind.hpp"
#include "post/storage/StorageCell.hpp"
#include <map>

namespace post {
// StorageArea — зона склада: собственные ячейки и правила совместимости с видами отправлений.
// Выбирает первую подходящую ячейку и размещает/извлекает записи через StorageCell.
// План группы строится на копии ячеек, чтобы учесть общий груз без изменения живого склада.
class StorageArea {
public:
    StorageArea(const std::string& id, const std::string& name, StorageAreaKind kind,
                std::vector<StorageCell> cells, std::vector<ShipmentKind> allowedShipmentKinds);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] StorageAreaKind kind() const noexcept;
    [[nodiscard]] const std::vector<StorageCell>& cells() const noexcept;
    [[nodiscard]] const std::vector<ShipmentKind>& allowedShipmentKinds() const noexcept;
    [[nodiscard]] bool contains(const std::string& shipmentId) const;
    [[nodiscard]] std::map<std::string, std::string>
    planPlacement(const std::vector<std::reference_wrapper<const Shipment>>& shipments,
                  const StoragePolicy& policy, std::chrono::sys_seconds time) const;
    [[nodiscard]] std::map<ShipmentKind, std::size_t>
    occupancyByKind(const ShipmentRegistry& registry) const;
    void place(const Shipment& shipment, const StoragePolicy& policy,
               std::chrono::sys_seconds time);
    [[nodiscard]] StorageEntry take(const std::string& shipmentId);
    bool operator==(const StorageArea& other) const = default;

private:
    void requireCompatible(const Shipment& shipment) const;
    std::string id_;
    std::string name_;
    StorageAreaKind kind_;
    std::vector<StorageCell> cells_;
    std::vector<ShipmentKind> allowedShipmentKinds_;
};
} // namespace post
