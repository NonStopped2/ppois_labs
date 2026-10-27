#pragma once
#include "post/core/Money.hpp"

namespace post {

// Интервал (lower, upper]; базовая цена включает первый весовой шаг.
class WeightBand {
public:
    WeightBand(std::int64_t lowerExclusiveGrams, std::int64_t upperInclusiveGrams, Money basePrice,
               std::int64_t stepGrams, Money stepPrice);
    [[nodiscard]] std::int64_t lowerExclusiveGrams() const noexcept;
    [[nodiscard]] std::int64_t upperInclusiveGrams() const noexcept;
    [[nodiscard]] const Money& basePrice() const noexcept;
    [[nodiscard]] std::int64_t stepGrams() const noexcept;
    [[nodiscard]] const Money& stepPrice() const noexcept;
    [[nodiscard]] bool contains(std::int64_t weightGrams) const noexcept;
    [[nodiscard]] Money priceFor(std::int64_t weightGrams) const;
    [[nodiscard]] bool overlaps(const WeightBand& other) const noexcept;
    bool operator==(const WeightBand& other) const = default;

private:
    std::int64_t lowerExclusiveGrams_;
    std::int64_t upperInclusiveGrams_;
    Money basePrice_;
    std::int64_t stepGrams_;
    Money stepPrice_;
};

} // namespace post
