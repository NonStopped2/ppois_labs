#pragma once

#include <compare>
#include <cstdint>
#include <string>

namespace shop {

/** BYN amounts in integer kopecks; signed values also represent report balances. */
class Money {
  public:
    explicit Money(std::int64_t kopecks = 0) noexcept;
    std::int64_t kopecks() const noexcept;
    Money checkedAdd(Money other) const;
    Money checkedSubtract(Money other) const;
    Money multiply(std::int64_t quantity) const;
    Money fractionFloor(std::int64_t numerator, std::int64_t denominator) const;
    Money percentageFloor(std::int64_t basisPoints) const;
    std::string format() const;
    auto operator<=>(const Money&) const = default;

  private:
    std::int64_t kopecks_;
};

} // namespace shop
