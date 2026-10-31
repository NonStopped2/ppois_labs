#pragma once

#include "post/registry/ShipmentRegistry.hpp"
#include "post/shipments/Letter.hpp"
#include "post/shipments/Parcel.hpp"

namespace post::test {

// Общие проверенные примеры сокращают подготовку тестов, но не вычисляют ожидаемый результат.
inline Customer customer(const std::string& id = "client",
                         const std::string& postalCode = "220000") {
    return Customer(id, "Алексей Тестов", PostalAddress(postalCode, "Минск", "Лесная", "1"),
                    ContactDetails("+375291234567"));
}
inline Letter letter() {
    return Letter(customer("sender"), customer("recipient"), 20, DeliveryOptions{},
                  Dimensions(200, 100, 5), 2, " Документы ");
}
inline Parcel parcel() {
    Parcel result(customer("sender"), customer("recipient"), 500, DeliveryOptions{},
                  Dimensions(200, 100, 50), PackagingKind::Box);
    result.addItem(ContentItem("Книга", 2, 100, Money(200), ContentCategory::GeneralGoods));
    result.seal("seal-1");
    return result;
}
inline std::string registerLetter(ShipmentRegistry& registry) {
    return registry.registerShipment(letter(), 1000, Dimensions(300, 200, 20), 100);
}
inline void advance(Shipment& shipment, TrackingEventKind kind, const std::string& branch = "A") {
    const auto& history = shipment.history().events();
    const auto number = history.empty() ? 1 : history.back().sequence() + 1;
    shipment.applyTrackingEvent(
        TrackingEvent(number, kind, std::chrono::sys_seconds{std::chrono::seconds{number}}, branch,
                      "Проверочное событие"));
}
inline ShipmentRegistry atStatus(ShipmentStatus target) {
    ShipmentRegistry registry("BY");
    const auto id = registerLetter(registry);
    auto& shipment = registry.at(id);
    if (target == ShipmentStatus::Draft) {
        return registry;
    }
    if (target == ShipmentStatus::Cancelled) {
        shipment.cancelBeforeAcceptance(std::chrono::sys_seconds{std::chrono::seconds{1}},
                                        "Отмена");
        return registry;
    }
    advance(shipment, TrackingEventKind::Accepted);
    if (target == ShipmentStatus::Accepted) {
        return registry;
    }
    if (target == ShipmentStatus::Lost) {
        advance(shipment, TrackingEventKind::Lost);
        return registry;
    }
    advance(shipment, TrackingEventKind::PreparedForDispatch);
    if (target == ShipmentStatus::PreparedForDispatch) {
        return registry;
    }
    advance(shipment, TrackingEventKind::Dispatched);
    if (target == ShipmentStatus::InTransit) {
        return registry;
    }
    advance(shipment, TrackingEventKind::Arrived, "B");
    if (target == ShipmentStatus::Arrived) {
        return registry;
    }
    if (target == ShipmentStatus::Returning || target == ShipmentStatus::Returned) {
        advance(shipment, TrackingEventKind::ReturnStarted, "B");
        advance(shipment, TrackingEventKind::PreparedForDispatch, "B");
        advance(shipment, TrackingEventKind::Dispatched, "B");
        advance(shipment, TrackingEventKind::Arrived, "A");
        if (target == ShipmentStatus::Returned) {
            advance(shipment, TrackingEventKind::Returned);
        }
        return registry;
    }
    if (target == ShipmentStatus::OutForDelivery) {
        advance(shipment, TrackingEventKind::OutForDelivery, "B");
        return registry;
    }
    advance(shipment, TrackingEventKind::AwaitingPickup, "B");
    if (target == ShipmentStatus::Delivered) {
        advance(shipment, TrackingEventKind::Delivered, "B");
    }
    return registry;
}

} // namespace post::test
