#pragma once

#include <cstdint>

namespace post {

// Габариты конверта или посылки для проверки размеров и расчёта объёма.
// Положительные стороны хранятся по убыванию: ориентация упаковки не влияет на сравнение.
class Dimensions {
public:
    Dimensions(std::int64_t lengthMm, std::int64_t widthMm, std::int64_t heightMm);

    [[nodiscard]] std::int64_t lengthMm() const noexcept;
    [[nodiscard]] std::int64_t widthMm() const noexcept;
    [[nodiscard]] std::int64_t heightMm() const noexcept;
    [[nodiscard]] bool fitsWithin(const Dimensions& limit) const noexcept;
    [[nodiscard]] std::int64_t volumeCubicMm() const;
    [[nodiscard]] std::int64_t lengthPlusGirthMm() const;

    bool operator==(const Dimensions& other) const = default;

private:
    std::int64_t lengthMm_;
    std::int64_t widthMm_;
    std::int64_t heightMm_;
};

} // namespace post
