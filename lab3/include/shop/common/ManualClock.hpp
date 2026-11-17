#pragma once

#include "shop/common/Timestamp.hpp"

namespace shop {

/** ManualClock.
 * Хранит текущее учебное время. Проверяет неотрицательность и переполнение при переводе
 * часов.
 */
class ManualClock {
  public:
    explicit ManualClock(Timestamp initial = 1767225600);
    Timestamp now() const noexcept;
    void advance(std::int64_t seconds);
    bool operator==(const ManualClock&) const = default;

  private:
    Timestamp now_;
};

} // namespace shop
