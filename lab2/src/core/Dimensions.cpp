#include "post/core/Dimensions.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace post {
namespace {

constexpr std::int64_t kGirthMultiplier = 2;

std::int64_t checkedProduct(std::int64_t first, std::int64_t second) {
    // Здесь оба множителя положительны благодаря инварианту Dimensions.
    if (first > std::numeric_limits<std::int64_t>::max() / second) {
        throw std::overflow_error("Dimensions multiplication overflow");
    }
    return first * second;
}

std::int64_t checkedSum(std::int64_t first, std::int64_t second) {
    if (first > std::numeric_limits<std::int64_t>::max() - second) {
        throw std::overflow_error("Dimensions addition overflow");
    }
    return first + second;
}

} // namespace

Dimensions::Dimensions(std::int64_t lengthMm, std::int64_t widthMm, std::int64_t heightMm)
    : lengthMm_(lengthMm), widthMm_(widthMm), heightMm_(heightMm) {
    if (lengthMm <= 0 || widthMm <= 0 || heightMm <= 0) {
        throw std::invalid_argument("Every dimension must be positive");
    }
    std::array sides{lengthMm, widthMm, heightMm};
    std::sort(sides.begin(), sides.end());
    lengthMm_ = sides[2];
    widthMm_ = sides[1];
    heightMm_ = sides[0];
}

std::int64_t Dimensions::lengthMm() const noexcept {
    return lengthMm_;
}

std::int64_t Dimensions::widthMm() const noexcept {
    return widthMm_;
}

std::int64_t Dimensions::heightMm() const noexcept {
    return heightMm_;
}

bool Dimensions::fitsWithin(const Dimensions& limit) const noexcept {
    // После сортировки достаточно сравнить соответствующие стороны.
    return lengthMm_ <= limit.lengthMm_ && widthMm_ <= limit.widthMm_ &&
           heightMm_ <= limit.heightMm_;
}

std::int64_t Dimensions::volumeCubicMm() const {
    const auto baseArea = checkedProduct(lengthMm_, widthMm_);
    return checkedProduct(baseArea, heightMm_);
}

std::int64_t Dimensions::lengthPlusGirthMm() const {
    const auto crossSectionSum = checkedSum(widthMm_, heightMm_);
    const auto girth = checkedProduct(crossSectionSum, kGirthMultiplier);
    return checkedSum(lengthMm_, girth);
}

} // namespace post
