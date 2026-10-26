#include "post/core/Dimensions.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

using post::Dimensions;

TEST_CASE("Dimensions canonicalize all orientations", "[dimensions]") {
    std::array<std::int64_t, 3> sides{10, 20, 30};
    do {
        const Dimensions box(sides[0], sides[1], sides[2]);
        REQUIRE(box.lengthMm() == 30);
        REQUIRE(box.widthMm() == 20);
        REQUIRE(box.heightMm() == 10);
        REQUIRE(box == Dimensions(30, 20, 10));
    } while (std::next_permutation(sides.begin(), sides.end()));
}

TEST_CASE("Dimensions reject nonpositive sides", "[dimensions]") {
    const std::array<std::int64_t, 3> invalid{0, -1, std::numeric_limits<std::int64_t>::min()};
    for (const auto side : invalid) {
        CAPTURE(side);
        REQUIRE_THROWS_AS(Dimensions(side, 1, 1), std::invalid_argument);
        REQUIRE_THROWS_AS(Dimensions(1, side, 1), std::invalid_argument);
        REQUIRE_THROWS_AS(Dimensions(1, 1, side), std::invalid_argument);
    }
}

TEST_CASE("Dimensions check each rotated size limit", "[dimensions]") {
    const Dimensions box(30, 20, 10);
    REQUIRE(box.fitsWithin(Dimensions(10, 30, 20)));
    REQUIRE(box.fitsWithin(Dimensions(31, 21, 11)));
    REQUIRE_FALSE(box.fitsWithin(Dimensions(29, 20, 10)));
    REQUIRE_FALSE(box.fitsWithin(Dimensions(30, 19, 10)));
    REQUIRE_FALSE(box.fitsWithin(Dimensions(30, 20, 9)));
}

TEST_CASE("Dimensions calculate volume and length plus girth", "[dimensions]") {
    const Dimensions box(30, 20, 10);
    // Независимые ожидаемые значения: 30*20*10 и 30 + 2*(20+10).
    REQUIRE(box.volumeCubicMm() == 6000);
    REQUIRE(box.lengthPlusGirthMm() == 90);
    REQUIRE(Dimensions(1, 1, 1).lengthPlusGirthMm() == 5);
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    REQUIRE(Dimensions(maximum, 1, 1).volumeCubicMm() == maximum);
    REQUIRE(Dimensions(maximum - 4, 1, 1).lengthPlusGirthMm() == maximum);
}

TEST_CASE("Dimensions detect overflow at every arithmetic stage", "[dimensions]") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const Dimensions large(maximum, 2, 1);
    REQUIRE_THROWS_AS(large.volumeCubicMm(), std::overflow_error);
    REQUIRE_THROWS_AS(Dimensions(3000000, 3000000, 3000000).volumeCubicMm(), std::overflow_error);
    REQUIRE_THROWS_AS(Dimensions(maximum, maximum, 1).lengthPlusGirthMm(), std::overflow_error);
    REQUIRE_THROWS_AS(Dimensions(maximum, maximum / 2, 2).lengthPlusGirthMm(), std::overflow_error);
    REQUIRE_THROWS_AS(Dimensions(maximum, 1, 1).lengthPlusGirthMm(), std::overflow_error);
    REQUIRE(large == Dimensions(maximum, 2, 1));
}

TEST_CASE("Dimensions copies are independent", "[dimensions]") {
    const Dimensions original(30, 20, 10);
    auto copy = original;
    copy = Dimensions(3, 2, 1);
    REQUIRE(original == Dimensions(30, 20, 10));
    REQUIRE(copy != original);
}
