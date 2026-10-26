#include "post/tracking/TrackingEvent.hpp"

#include "post/detail/Text.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace post {
namespace {

// Даже enum может содержать неизвестное значение после static_cast: проверяем явно.
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

// Сообщение, отмена и утрата допускаются без привязки к конкретному отделению.
bool requiresBranch(TrackingEventKind kind) {
    return kind != TrackingEventKind::Information && kind != TrackingEventKind::Cancelled &&
           kind != TrackingEventKind::Lost;
}

// Для сообщения и проблемного исхода одного вида события недостаточно: нужно объяснение.
bool requiresDescription(TrackingEventKind kind) {
    return kind == TrackingEventKind::DeliveryFailed || kind == TrackingEventKind::Information ||
           kind == TrackingEventKind::Cancelled || kind == TrackingEventKind::Lost;
}

} // namespace

// Нормализует текст и создаёт событие только с допустимым составом данных.
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

// Порядковый номер внутри истории, а не трек-номер самого отправления.
std::int64_t TrackingEvent::sequence() const noexcept {
    return sequence_;
}

// Вид факта определяет обязательные данные и его влияние на учёт местоположения.
TrackingEventKind TrackingEvent::kind() const noexcept {
    return kind_;
}

// Явно переданное время факта в секундах Unix, а не время чтения объекта.
std::chrono::sys_seconds TrackingEvent::occurredAt() const noexcept {
    return occurredAt_;
}

// optional сохраняет различие между известным отделением и допустимым отсутствием места.
const std::optional<std::string>& TrackingEvent::branchId() const noexcept {
    return branchId_;
}

// Пояснение для журнала: в частности, причина неудачной доставки, отмены или утраты.
const std::string& TrackingEvent::description() const noexcept {
    return description_;
}

// Проверяет отдельный факт; хронологию относительно других событий проверяет история.
void TrackingEvent::validate() const {
    // Неотрицательные метки позволяют безопасно вычитать и суммировать интервалы.
    if (sequence_ <= 0 || !supportedKind(kind_) || occurredAt_ < std::chrono::sys_seconds{}) {
        throw InvalidShipmentDataException(
            "Event requires a positive sequence, known kind and nonnegative Unix time");
    }
    // optional разыменовывается только после проверки наличия значения через &&.
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
