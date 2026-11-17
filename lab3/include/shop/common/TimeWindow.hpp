#pragma once

#include "shop/common/Timestamp.hpp"

namespace shop {

/** Half-open interval [startsAt, endsAt) in seconds of the educational clock. */
class TimeWindow {
  public:
    TimeWindow(Timestamp startsAt, Timestamp endsAt);
    Timestamp startsAt() const noexcept;
    Timestamp endsAt() const noexcept;
    bool contains(Timestamp time) const noexcept;
    bool overlaps(const TimeWindow& other) const noexcept;
    bool operator==(const TimeWindow&) const = default;

  private:
    Timestamp startsAt_;
    Timestamp endsAt_;
};

} // namespace shop
