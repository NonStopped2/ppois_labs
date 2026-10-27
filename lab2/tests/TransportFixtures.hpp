#pragma once
#include "BranchFixtures.hpp"
#include "post/returns/SearchRequest.hpp"
#include "post/transport/TransportManifest.hpp"

namespace post::test {
inline Vehicle vehicle(const std::string& id = "car", std::int64_t weight = 10000,
                       std::int64_t bags = 10) {
    return Vehicle(id, "AB-123", weight, bags, VehicleType::Van);
}
inline VehicleInspection inspection(const Vehicle& car) {
    VehicleInspection result("inspection", car, moment(0), moment(10000));
    result.approve(employee("inspector", "A", EmployeeRole::Inspector), moment(0));
    return result;
}
inline DispatchBatch batchFor(const ShipmentRegistry& registry, const PostalBranch& target,
                              const std::vector<std::string>& ids,
                              const std::string& key = "batch") {
    const SortingPlan plan("sorting", "A",
                           {SortingRule("target", {"22", "23", "24"}, target.id(), 1,
                                        {ShipmentKind::Letter, ShipmentKind::Parcel})},
                           1);
    MailBag bag("bag-" + key, 10000, target.id());
    for (const auto& id : ids) {
        bag.add(registry.at(id), target, plan);
    }
    bag.seal("seal-" + key, moment(20), registry);
    DispatchBatch result(key, "A", target.id(), moment(20));
    result.addBag(bag);
    result.prepareForDispatch(routeBetween("A", target.id()));
    return result;
}
} // namespace post::test
