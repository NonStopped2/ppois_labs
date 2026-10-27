#include "post/transport/RouteStop.hpp"
#include "post/detail/Time.hpp"
#include "post/detail/Validation.hpp"

namespace post {
// Первая остановка не имеет предыдущего участка; оба интервала представимы в секундах.
RouteStop::RouteStop(const std::string& branchId, std::int64_t sequence,
                     std::chrono::minutes travelMinutesFromPrevious,
                     std::chrono::minutes handlingMinutes)
    : branchId_(detail::identifier(branchId)), sequence_(sequence),
      travelMinutesFromPrevious_(travelMinutesFromPrevious), handlingMinutes_(handlingMinutes) {
    (void)detail::secondsFromMinutes(travelMinutesFromPrevious_);
    (void)detail::secondsFromMinutes(handlingMinutes_);
    if (sequence_ <= 0 || (sequence_ == 1 && travelMinutesFromPrevious_.count() != 0)) {
        throw std::invalid_argument("Invalid route stop position");
    }
}
// Здесь хранится идентификатор, а не независимая копия отделения.
const std::string& RouteStop::branchId() const noexcept {
    return branchId_;
}
// Позиции маршрута обязаны идти подряд, начиная с единицы.
std::int64_t RouteStop::sequence() const noexcept {
    return sequence_;
}
// Длительность движения отсчитывается от предыдущего пункта.
std::chrono::minutes RouteStop::travelMinutesFromPrevious() const noexcept {
    return travelMinutesFromPrevious_;
}
// Обработка начинается после прибытия и допускает нулевой интервал.
std::chrono::minutes RouteStop::handlingMinutes() const noexcept {
    return handlingMinutes_;
}
// Не прибавляет дорогу повторно: arrival уже является моментом прибытия в этот пункт.
std::chrono::sys_seconds RouteStop::earliestDeparture(std::chrono::sys_seconds arrival) const {
    return detail::after(arrival, detail::secondsFromMinutes(handlingMinutes_));
}
} // namespace post
