#pragma once
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/shipments/Shipment.hpp"
#include <algorithm>
#include <limits>

namespace post::detail {
// Служебный запрос последнего неинформационного факта для фазы возврата.
inline TrackingEventKind lastOperationalFact(const Shipment& shipment) {
    const auto& events = shipment.history().events();
    const auto last = std::find_if(events.rbegin(), events.rend(), [](const auto& event) {
        return event.kind() != TrackingEventKind::Information;
    });
    if (last == events.rend()) {
        throw InvalidShipmentStateException("Shipment has no operational history");
    }
    return last->kind();
}
// Не вычисляет sequence + 1, пока не проверено переполнение; автомат Shipment остаётся главным.
inline void appendFact(Shipment& shipment, TrackingEventKind kind, std::chrono::sys_seconds time,
                       const std::string& branchId, const std::string& description = "") {
    const auto& events = shipment.history().events();
    const auto sequence = events.empty() ? 0 : events.back().sequence();
    if (sequence == std::numeric_limits<std::int64_t>::max()) {
        throw std::overflow_error("Tracking sequence exhausted");
    }
    shipment.applyTrackingEvent(TrackingEvent(sequence + 1, kind, time, branchId, description));
}
// Возврат можно инициировать только пока отправление физически хранится, не в пути.
inline void requireReturnable(const Shipment& shipment) {
    const auto status = shipment.status();
    if (status != ShipmentStatus::Accepted && status != ShipmentStatus::PreparedForDispatch &&
        status != ShipmentStatus::Arrived && status != ShipmentStatus::AwaitingPickup) {
        throw InvalidShipmentStateException("Shipment unavailable for a new return request");
    }
}
} // namespace post::detail
