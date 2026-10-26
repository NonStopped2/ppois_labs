#pragma once

#include <cstdint>

namespace post {

// Стороны хранятся по убыванию: поворот коробки не меняет её габариты.
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
