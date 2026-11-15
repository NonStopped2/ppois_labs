#pragma once

#include <cstdint>

namespace shop {

using Timestamp = std::int64_t;

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

/** Deterministic time source; loading constructs a clock at the saved timestamp. */
class ManualClock {
  public:
    explicit ManualClock(Timestamp initial = 1767225600);
    Timestamp now() const noexcept;
    void advance(std::int64_t seconds);

  private:
    Timestamp now_;
};

} // namespace shop
