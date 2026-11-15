#include "shop/common/TimeWindow.hpp"
#include "shop/common/Validation.hpp"

namespace shop {

TimeWindow::TimeWindow(Timestamp startsAt, Timestamp endsAt)
    : startsAt_(startsAt), endsAt_(endsAt) {
    detail::require(startsAt >= 0 && endsAt > startsAt, "Некорректный интервал времени");
}

Timestamp TimeWindow::startsAt() const noexcept {
    return startsAt_;
}
Timestamp TimeWindow::endsAt() const noexcept {
    return endsAt_;
}
bool TimeWindow::contains(Timestamp time) const noexcept {
    return time >= startsAt_ && time < endsAt_;
}
bool TimeWindow::overlaps(const TimeWindow& other) const noexcept {
    return startsAt_ < other.endsAt_ && other.startsAt_ < endsAt_;
}

ManualClock::ManualClock(Timestamp initial) : now_(initial) {
    detail::require(initial >= 0, "Учебное время не может быть отрицательным");
}

Timestamp ManualClock::now() const noexcept {
    return now_;
}
void ManualClock::advance(std::int64_t seconds) {
    now_ = detail::addNonnegative(now_, seconds);
}

} // namespace shop
