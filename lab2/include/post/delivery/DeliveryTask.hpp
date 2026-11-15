#pragma once
#include "post/core/Customer.hpp"
#include "post/core/PostalAddress.hpp"
#include "post/delivery/DeliveryAttempt.hpp"
#include "post/delivery/DeliveryTaskState.hpp"
#include "post/delivery/PickupAuthorization.hpp"
#include "post/registry/ShipmentRegistry.hpp"
#include "post/storage/StorageEntry.hpp"
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace post {
class Courier;
class PostalBranch;
// DeliveryTask — доставка одного отправления: получатель, адрес, окно, курьер и журнал попыток.
// Проверяет время, StoragePolicy и личность при вручении; принадлежит DeliveryRoute.
// Повтор сохраняет ID задания, реальные попытки и исходный дедлайн, а не начинает цикл заново.
class DeliveryTask {
public:
    using Window = std::pair<std::chrono::sys_seconds, std::chrono::sys_seconds>;
    DeliveryTask(const std::string& id, const Shipment& shipment, const StorageEntry& entry);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& shipmentId() const noexcept;
    [[nodiscard]] const Customer& recipient() const noexcept;
    [[nodiscard]] const PostalAddress& address() const noexcept;
    [[nodiscard]] std::int64_t weightGrams() const noexcept;
    [[nodiscard]] const Window& scheduledWindow() const noexcept;
    [[nodiscard]] const std::vector<DeliveryAttempt>& attempts() const noexcept;
    [[nodiscard]] DeliveryTaskState state() const noexcept;
    [[nodiscard]] const std::string& courierId() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds storageReceivedAt() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds storageDeadlineAt() const noexcept;
    void schedule(Window window, const Courier& courier, const Shipment& shipment,
                  const StoragePolicy& policy);
    void recordAttempt(ShipmentRegistry& registry, const Courier& courier,
                       const PostalBranch& branch, const DeliveryAttempt& attempt,
                       const StoragePolicy& policy);
    void completeDelivery(ShipmentRegistry& registry, const Courier& courier,
                          const PostalBranch& branch, const DeliveryAttempt& attempt,
                          const StoragePolicy& policy, const Customer& claimant,
                          const IdentityDocument& document,
                          const PickupAuthorization* authorization = nullptr);
    void reschedule(Window window, const Courier& courier, const Shipment& shipment,
                    const StoragePolicy& policy);
    bool operator==(const DeliveryTask& other) const = default;

private:
    friend class Courier;
    void validateShipment(const Shipment& shipment) const;
    void validateAttempt(const Shipment& shipment, const Courier& courier,
                         const PostalBranch& branch, const DeliveryAttempt& attempt,
                         const StoragePolicy& policy) const;
    void validateWindow(Window window, const Courier& courier, const Shipment& shipment,
                        const StoragePolicy& policy) const;
    std::string id_;
    std::string shipmentId_;
    Customer recipient_;
    PostalAddress address_;
    std::int64_t weightGrams_;
    Window scheduledWindow_{};
    std::vector<DeliveryAttempt> attempts_;
    DeliveryTaskState state_{DeliveryTaskState::Draft};
    std::string courierId_;
    std::chrono::sys_seconds storageReceivedAt_;
    std::chrono::sys_seconds storageDeadlineAt_;
};
} // namespace post
