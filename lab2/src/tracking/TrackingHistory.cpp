#include "post/tracking/TrackingHistory.hpp"

#include "post/detail/Text.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <stdexcept>

namespace post {
namespace {

// Виды событий, которые начинают новый интервал нахождения в отделении.
bool entersBranch(TrackingEventKind kind) {
    return kind == TrackingEventKind::Accepted || kind == TrackingEventKind::Arrived ||
           kind == TrackingEventKind::AwaitingPickup || kind == TrackingEventKind::Returned;
}

// Виды событий, завершающие интервал: выбытие, вручение, отмена или утрата.
bool leavesBranch(TrackingEventKind kind) {
    return kind == TrackingEventKind::Dispatched || kind == TrackingEventKind::OutForDelivery ||
           kind == TrackingEventKind::Delivered || kind == TrackingEventKind::Cancelled ||
           kind == TrackingEventKind::Lost;
}

// Ищет конец одного пребывания; информационные события не закрывают интервал.
std::chrono::sys_seconds stayEndsAt(const std::vector<TrackingEvent>& events, std::size_t index,
                                    std::chrono::sys_seconds asOf) {
    // Начинаем после входа, иначе он сам закрыл бы свой интервал нулевой длительностью.
    for (++index; index < events.size(); ++index) {
        if (entersBranch(events[index].kind()) || leavesBranch(events[index].kind())) {
            // Будущее событие не должно продлевать пребывание за момент наблюдения.
            return std::min(events[index].occurredAt(), asOf);
        }
    }
    return asOf;
}

} // namespace

// Даёт журнал только для чтения, чтобы нельзя было обойти проверки append.
const std::vector<TrackingEvent>& TrackingHistory::events() const noexcept {
    return events_;
}

// Добавляет копию факта после проверок, сохраняя прежний журнал при бизнес-ошибке.
void TrackingHistory::append(const TrackingEvent& event) {
    event.validate();
    // back() допустим только у непустого вектора; && не вычисляет его для пустой истории.
    if (!events_.empty() && (event.sequence() <= events_.back().sequence() ||
                             event.occurredAt() < events_.back().occurredAt())) {
        throw InvalidShipmentStateException(
            "Tracking event sequence must increase and time cannot decrease");
    }
    // Все бизнес-проверки выполнены до изменения; vector копирует событие целиком.
    events_.push_back(event);
}

// Возвращает независимую выборку за интервал с включёнными границами.
std::vector<TrackingEvent> TrackingHistory::eventsBetween(std::chrono::sys_seconds from,
                                                          std::chrono::sys_seconds to) const {
    if (from < std::chrono::sys_seconds{} || to < from) {
        throw std::invalid_argument("Invalid tracking history interval");
    }
    std::vector<TrackingEvent> result;
    // back_inserter добавляет подходящие события в новый вектор, не меняя исходный.
    std::copy_if(events_.begin(), events_.end(), std::back_inserter(result),
                 [from, to](const TrackingEvent& event) {
                     return from <= event.occurredAt() && event.occurredAt() <= to;
                 });
    return result;
}

// Находит последнее место из фактов перемещения, игнорируя обычные сообщения.
std::optional<std::string> TrackingHistory::lastKnownBranch() const {
    // Обратные итераторы позволяют начать с конца журнала, то есть с последних фактов.
    for (auto event = events_.rbegin(); event != events_.rend(); ++event) {
        const auto kind = event->kind();
        if ((entersBranch(kind) || leavesBranch(kind)) && event->branchId()) {
            return event->branchId();
        }
    }
    return std::nullopt;
}

// Суммирует пребывания в нужном отделении до asOf, включая незавершённый интервал.
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
