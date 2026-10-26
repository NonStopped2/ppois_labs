#pragma once

namespace post {

enum class TrackingEventKind {
    Accepted,
    PreparedForDispatch,
    Dispatched,
    Arrived,
    OutForDelivery,
    AwaitingPickup,
    DeliveryFailed,
    Delivered,
    ReturnStarted,
    Returned,
    Cancelled,
    Lost,
    Information
};

} // namespace post
