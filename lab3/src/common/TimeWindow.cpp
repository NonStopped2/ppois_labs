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

} // namespace shop
