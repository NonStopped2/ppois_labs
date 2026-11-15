#pragma once
#include "post/shipments/Shipment.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace post {

// ShipmentRegistry — владелец зарегистрированных отправлений, поиск по трек-номеру.
// Хранит активные и архивные объекты через unique_ptr<Shipment>; архив остаётся доступным.
// При регистрации назначает номер своей копии черновика, не изменяя исходный объект.
// Копирование реестра вызывает clone(), сохраняя типы Letter/Parcel и независимость копий.
class ShipmentRegistry {
public:
    explicit ShipmentRegistry(const std::string& trackingPrefix, std::int64_t firstSequence = 1);
    ShipmentRegistry(const ShipmentRegistry& other);
    ShipmentRegistry& operator=(const ShipmentRegistry& other);
    ShipmentRegistry(ShipmentRegistry&& other) noexcept = default;
    ShipmentRegistry& operator=(ShipmentRegistry&& other) noexcept = default;
    [[nodiscard]] std::string registerShipment(const Shipment& prototype,
                                               std::int64_t maxWeightGrams,
                                               const Dimensions& sizeLimit, std::int64_t maxUnits);
    [[nodiscard]] Shipment& at(const std::string& trackingNumber);
    [[nodiscard]] const Shipment& at(const std::string& trackingNumber) const;
    [[nodiscard]] std::vector<std::reference_wrapper<const Shipment>>
    findByStatus(ShipmentStatus status) const;
    [[nodiscard]] std::vector<std::reference_wrapper<const Shipment>>
    findByCustomer(const Customer& customer) const;
    std::size_t archiveCompletedBefore(std::chrono::sys_seconds cutoff);
    [[nodiscard]] std::size_t activeCount() const noexcept;
    [[nodiscard]] std::size_t archivedCount() const noexcept;
    [[nodiscard]] std::int64_t nextSequence() const noexcept;
    [[nodiscard]] const std::string& trackingPrefix() const noexcept;

private:
    using Store = std::map<std::string, std::unique_ptr<Shipment>>;
    [[nodiscard]] static Store cloneStore(const Store& original);
    void swap(ShipmentRegistry& other) noexcept;
    Store shipments_;
    Store archivedShipments_;
    std::int64_t nextSequence_;
    std::string trackingPrefix_;
};

} // namespace post
