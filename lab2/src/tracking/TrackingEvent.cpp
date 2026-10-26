#include "post/tracking/TrackingEvent.hpp"

#include "post/detail/Text.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace post {
namespace {

bool supportedKind(TrackingEventKind kind) {
    constexpr std::array kinds{
        TrackingEventKind::Accepted,       TrackingEventKind::PreparedForDispatch,
        TrackingEventKind::Dispatched,     TrackingEventKind::Arrived,
        TrackingEventKind::OutForDelivery, TrackingEventKind::AwaitingPickup,
        TrackingEventKind::DeliveryFailed, TrackingEventKind::Delivered,
        TrackingEventKind::ReturnStarted,  TrackingEventKind::Returned,
        TrackingEventKind::Cancelled,      TrackingEventKind::Lost,
        TrackingEventKind::Information};
    return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
}

bool requiresBranch(TrackingEventKind kind) {
    return kind != TrackingEventKind::Information && kind != TrackingEventKind::Cancelled &&
           kind != TrackingEventKind::Lost;
}

bool requiresDescription(TrackingEventKind kind) {
    return kind == TrackingEventKind::DeliveryFailed || kind == TrackingEventKind::Information ||
           kind == TrackingEventKind::Cancelled || kind == TrackingEventKind::Lost;
}

} // namespace

TrackingEvent::TrackingEvent(std::int64_t sequence, TrackingEventKind kind,
                             std::chrono::sys_seconds occurredAt,
                             std::optional<std::string> branchId, const std::string& description)
    : sequence_(sequence), kind_(kind), occurredAt_(occurredAt), branchId_(std::move(branchId)),
      description_(detail::normalizeText(description)) {
    if (branchId_) {
        *branchId_ = detail::normalizeText(*branchId_);
    }
    validate();
}

std::int64_t TrackingEvent::sequence() const noexcept {
    return sequence_;
}

TrackingEventKind TrackingEvent::kind() const noexcept {
    return kind_;
}

std::chrono::sys_seconds TrackingEvent::occurredAt() const noexcept {
    return occurredAt_;
}

const std::optional<std::string>& TrackingEvent::branchId() const noexcept {
    return branchId_;
}

const std::string& TrackingEvent::description() const noexcept {
    return description_;
}

void TrackingEvent::validate() const {
    // Неотрицательные метки позволяют безопасно вычитать и суммировать интервалы.
    if (sequence_ <= 0 || !supportedKind(kind_) || occurredAt_ < std::chrono::sys_seconds{}) {
        throw InvalidShipmentDataException(
            "Event requires a positive sequence, known kind and nonnegative Unix time");
    }
    if ((requiresBranch(kind_) && !branchId_) ||
        (branchId_ &&
         (!detail::hasPrintableText(*branchId_) || branchId_->find(' ') != std::string::npos))) {
        throw InvalidShipmentDataException("Event requires a valid branch identifier");
    }
    if ((requiresDescription(kind_) && description_.empty()) ||
        (!description_.empty() && !detail::hasPrintableText(description_))) {
        throw InvalidShipmentDataException(
            "Event description is missing or contains control characters");
    }
}

} // namespace post
