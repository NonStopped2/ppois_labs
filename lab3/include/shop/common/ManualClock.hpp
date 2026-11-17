#pragma once

#include "shop/common/Timestamp.hpp"

namespace shop {

/** Deterministic time source; loading constructs a clock at the saved timestamp. */
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
