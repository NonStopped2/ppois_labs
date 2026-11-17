#include "shop/common/ManualClock.hpp"
#include "shop/common/Validation.hpp"

namespace shop {

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
