#pragma once
#include "post/delivery/PickupAuthorization.hpp"
#include "post/payments/Invoice.hpp"
#include "post/personnel/WorkShift.hpp"
#include "post/sorting/DispatchBatch.hpp"
#include "post/storage/StorageArea.hpp"

namespace post {
// Владеет складом и персоналом, но не отправлениями. Согласует реестр, размещения и партии.
// Успешная операция заменяет снимки: ссылки на Shipment и элементы склада надо получить заново.
class PostalBranch {
public:
    PostalBranch(const std::string& id, const std::string& name, const PostalAddress& address,
                 std::vector<StorageArea> storageAreas, std::vector<Employee> employees,
                 std::vector<WorkShift> workShifts, std::vector<std::string> servedPostalPrefixes);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] const PostalAddress& address() const noexcept;
    [[nodiscard]] const std::vector<StorageArea>& storageAreas() const noexcept;
    [[nodiscard]] const std::vector<Employee>& employees() const noexcept;
    [[nodiscard]] const std::vector<WorkShift>& workShifts() const noexcept;
    [[nodiscard]] const std::vector<std::string>& servedPostalPrefixes() const noexcept;
    [[nodiscard]] bool serves(const PostalAddress& destination) const;
    [[nodiscard]] const StorageEntry& storageEntry(const std::string& trackingNumber) const;
    void authorize(const std::string& employeeId, BranchOperation operation,
                   std::chrono::sys_seconds time) const;
    void addShift(const WorkShift& shift);
    void startShift(const std::string& shiftId, std::chrono::sys_seconds time);
    void addShiftBreak(const std::string& shiftId, std::chrono::sys_seconds from,
                       std::chrono::sys_seconds to);
    void finishShift(const std::string& shiftId, std::chrono::sys_seconds time);
    void acceptShipment(ShipmentRegistry& registry, const Invoice& invoice,
                        const std::string& trackingNumber, const std::string& employeeId,
                        const StoragePolicy& policy, std::int64_t maxWeightGrams,
                        const Dimensions& sizeLimit, std::int64_t maxUnits,
                        std::chrono::sys_seconds time);
    void transferOut(ShipmentRegistry& registry, DispatchBatch& batch, const SortingPlan& plan,
                     const std::string& employeeId, std::chrono::sys_seconds time);
    void receiveBatch(ShipmentRegistry& registry, DispatchBatch& batch,
                      const std::string& employeeId, const StoragePolicy& policy,
                      std::chrono::sys_seconds time);
    void issueShipment(ShipmentRegistry& registry, const std::string& trackingNumber,
                       const std::string& employeeId, const Customer& claimant,
                       const IdentityDocument& document, std::chrono::sys_seconds time,
                       const PickupAuthorization* authorization = nullptr);
    bool operator==(const PostalBranch& other) const = default;

private:
    friend class ReturnRequest;
    friend class SearchRequest;
    friend class Courier;
    void sendForDelivery(ShipmentRegistry& registry, const std::string& trackingNumber,
                         const std::string& employeeId, std::chrono::sys_seconds time);
    void returnFromCourier(ShipmentRegistry& registry, const std::string& trackingNumber,
                           const std::string& employeeId, const StoragePolicy& policy,
                           std::chrono::sys_seconds time, bool interrupted = false);
    void confirmLoss(ShipmentRegistry& registry, const std::string& trackingNumber,
                     const std::string& employeeId, std::chrono::sys_seconds time);
    void beginReturn(ShipmentRegistry& registry, const std::string& trackingNumber,
                     const std::string& employeeId, std::chrono::sys_seconds time);
    void validateConfiguration() const;
    void validateShift(const WorkShift& shift) const;
    [[nodiscard]] const Employee& employee(const std::string& employeeId) const;
    [[nodiscard]] WorkShift& shift(const std::string& shiftId);
    void requireStored(const Shipment& shipment) const;
    void dispatchCargo(ShipmentRegistry& registry, std::vector<StorageArea>& areas,
                       const DispatchBatch& batch, const SortingPlan& plan,
                       std::chrono::sys_seconds time) const;
    static void placeIn(std::vector<StorageArea>& areas, const Shipment& shipment,
                        const StoragePolicy& policy, std::chrono::sys_seconds time);
    static void takeFrom(std::vector<StorageArea>& areas, const std::string& trackingNumber);
    static void commit(ShipmentRegistry& registry, ShipmentRegistry&& staged,
                       std::vector<StorageArea>& liveAreas,
                       std::vector<StorageArea>&& stagedAreas) noexcept;
    std::string id_;
    std::string name_;
    PostalAddress address_;
    std::vector<StorageArea> storageAreas_;
    std::vector<Employee> employees_;
    std::vector<WorkShift> workShifts_;
    std::vector<std::string> servedPostalPrefixes_;
};
} // namespace post
