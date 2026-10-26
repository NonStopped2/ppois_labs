#include "post/tracking/TrackingHistory.hpp"

#include "post/detail/Text.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <stdexcept>

namespace post {
namespace {

bool entersBranch(TrackingEventKind kind) {
    return kind == TrackingEventKind::Accepted || kind == TrackingEventKind::Arrived ||
           kind == TrackingEventKind::AwaitingPickup || kind == TrackingEventKind::Returned;
}

bool leavesBranch(TrackingEventKind kind) {
    return kind == TrackingEventKind::Dispatched || kind == TrackingEventKind::OutForDelivery ||
           kind == TrackingEventKind::Delivered || kind == TrackingEventKind::Cancelled ||
           kind == TrackingEventKind::Lost;
}

std::chrono::sys_seconds stayEndsAt(const std::vector<TrackingEvent>& events, std::size_t index,
                                    std::chrono::sys_seconds asOf) {
    for (++index; index < events.size(); ++index) {
        if (entersBranch(events[index].kind()) || leavesBranch(events[index].kind())) {
            return std::min(events[index].occurredAt(), asOf);
        }
    }
    return asOf;
}

} // namespace

const std::vector<TrackingEvent>& TrackingHistory::events() const noexcept {
    return events_;
}

void TrackingHistory::append(const TrackingEvent& event) {
    event.validate();
    if (!events_.empty() && (event.sequence() <= events_.back().sequence() ||
                             event.occurredAt() < events_.back().occurredAt())) {
        throw InvalidShipmentStateException(
            "Tracking event sequence must increase and time cannot decrease");
    }
    // Все бизнес-проверки выполнены до изменения; vector копирует событие целиком.
    events_.push_back(event);
}

std::vector<TrackingEvent> TrackingHistory::eventsBetween(std::chrono::sys_seconds from,
                                                          std::chrono::sys_seconds to) const {
    if (from < std::chrono::sys_seconds{} || to < from) {
        throw std::invalid_argument("Invalid tracking history interval");
    }
    std::vector<TrackingEvent> result;
    std::copy_if(events_.begin(), events_.end(), std::back_inserter(result),
                 [from, to](const TrackingEvent& event) {
                     return from <= event.occurredAt() && event.occurredAt() <= to;
                 });
    return result;
}

std::optional<std::string> TrackingHistory::lastKnownBranch() const {
    for (auto event = events_.rbegin(); event != events_.rend(); ++event) {
        const auto kind = event->kind();
        if ((entersBranch(kind) || leavesBranch(kind)) && event->branchId()) {
            return event->branchId();
        }
    }
    return std::nullopt;
}

std::chrono::seconds TrackingHistory::timeSpentAt(std::string_view branchId,
                                                  std::chrono::sys_seconds asOf) const {
    const std::string target = detail::normalizeText(branchId);
    if (!detail::hasPrintableText(target) || target.find(' ') != std::string::npos ||
        asOf < std::chrono::sys_seconds{}) {
        throw std::invalid_argument("Invalid branch identifier or observation time");
    }
    std::chrono::seconds total{0};
    for (std::size_t index = 0; index < events_.size() && events_[index].occurredAt() <= asOf;
         ++index) {
        const auto& event = events_[index];
        if (entersBranch(event.kind()) && event.branchId() == target) {
            // Интервалы не пересекаются: поиск их концов суммарно остаётся линейным.
            total += stayEndsAt(events_, index, asOf) - event.occurredAt();
        }
    }
    return total;
}

} // namespace post
