#pragma once
#include "TariffFixtures.hpp"
#include "post/storage/StorageArea.hpp"
#include "post/transport/PostalRoute.hpp"

namespace post::test {
inline StoragePolicy storagePolicy() {
    return StoragePolicy("policy", 1, 3, Money(50), 3, 1);
}
inline StorageArea storageArea(const std::string& id = "area", std::int64_t count = 10) {
    return StorageArea(id, "Общее хранение", StorageAreaKind::General,
                       {StorageCell("cell", 10000, count, Dimensions(1000, 1000, 1000))},
                       {ShipmentKind::Letter, ShipmentKind::Parcel});
}
inline PostalRoute route() {
    return PostalRoute("route", "Между отделениями",
                       {RouteStop("A", 1, std::chrono::minutes{0}, std::chrono::minutes{5}),
                        RouteStop("B", 2, std::chrono::minutes{20}, std::chrono::minutes{5})},
                       std::chrono::minutes{30}, {VehicleType::Van, VehicleType::Truck});
}
} // namespace post::test
