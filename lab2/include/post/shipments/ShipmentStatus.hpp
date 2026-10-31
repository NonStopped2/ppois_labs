#pragma once

namespace post {

enum class ShipmentStatus {
    Draft,
    Accepted,
    PreparedForDispatch,
    InTransit,
    Arrived,
    OutForDelivery,
    AwaitingPickup,
    Delivered,
    Returning,
    Returned,
    Cancelled,
    Lost
};

} // namespace post
