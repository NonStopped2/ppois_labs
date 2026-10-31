#pragma once
#include "PaymentFixtures.hpp"
#include "StaffFixtures.hpp"
#include "post/branches/PostalBranch.hpp"
#include "post/returns/ReturnRequest.hpp"
#include <limits>

namespace post::test {
inline PostalBranch branch(const std::string& id = "A", const std::string& prefix = "22",
                           std::int64_t capacity = 10, EmployeeRole role = EmployeeRole::Manager) {
    return PostalBranch(id, "Отделение " + id, customer("address", prefix + "0000").address(),
                        {storageArea("area", capacity)}, {employee("worker", id, role)},
                        {shift("shift", id)}, {prefix});
}
inline std::string registerCargo(ShipmentRegistry& registry, std::int64_t weight = 20) {
    return registry.registerShipment(
        Letter(customer("sender", "220000"), customer("recipient", "230000"), weight,
               DeliveryOptions{}, Dimensions(200, 100, 5), 2, "Документы"),
        std::numeric_limits<std::int64_t>::max(), Dimensions(1000, 1000, 1000), 100);
}
inline Invoice paidInvoice(const ShipmentRegistry& registry, const std::string& id) {
    auto result = invoice(registry.at(id));
    result.registerPayment(payment("payment", 100), registry.at(id));
    return result;
}
inline void accept(PostalBranch& branch, ShipmentRegistry& registry, const std::string& id,
                   const StoragePolicy& policy = storagePolicy()) {
    branch.acceptShipment(registry, paidInvoice(registry, id), id, "worker", policy, 1000,
                          Dimensions(1000, 1000, 1000), 100, moment(15));
}
inline PostalRoute routeBetween(const std::string& source, const std::string& target) {
    return PostalRoute("route", "Между отделениями",
                       {RouteStop(source, 1, std::chrono::minutes{0}, std::chrono::minutes{5}),
                        RouteStop(target, 2, std::chrono::minutes{20}, std::chrono::minutes{5})},
                       std::chrono::minutes{30}, {VehicleType::Van});
}
inline DispatchBatch preparedBatch(const ShipmentRegistry& registry,
                                   const PostalBranch& destination,
                                   const std::vector<std::string>& ids = {"BY-1"},
                                   const std::string& source = "A", std::int64_t sealedAt = 20) {
    MailBag bag("bag", 10000, destination.id());
    for (const auto& id : ids) {
        bag.add(registry.at(id), destination, sortingPlan(source, destination.id()));
    }
    bag.seal("seal", moment(sealedAt), registry);
    DispatchBatch batch("batch", source, destination.id(), moment(sealedAt));
    batch.addBag(bag);
    batch.prepareForDispatch(routeBetween(source, destination.id()));
    return batch;
}
} // namespace post::test
