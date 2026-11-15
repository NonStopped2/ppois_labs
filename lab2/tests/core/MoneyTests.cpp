#include "post/core/Money.hpp"
#include "post/exceptions/InvalidAmountException.hpp"
#include "post/exceptions/PostalException.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

using post::InvalidAmountException;
using post::Money;

TEST_CASE("Money stores exact kopecks and supports ordering", "[money]") {
    STATIC_REQUIRE_FALSE(std::is_convertible_v<std::int64_t, Money>);
    REQUIRE(Money().kopecks() == 0);
    REQUIRE(Money(1250).kopecks() == 1250);
    REQUIRE(Money(100) == Money(100));
    REQUIRE(Money(99) != Money(100));
    REQUIRE(Money(99) < Money(100));
    REQUIRE(Money(100) > Money(99));
    REQUIRE(Money(100) <= Money(100));
    REQUIRE(Money(100) >= Money(100));
}

TEST_CASE("Money arithmetic returns new values", "[money]") {
    const Money price(1250);
    const Money fee(150);
    REQUIRE(price.add(fee) == Money(1400));
    REQUIRE(price.subtract(fee) == Money(1100));
    REQUIRE(price.subtract(price) == Money());
    REQUIRE(price.multiply(3) == Money(3750));
    REQUIRE(price.multiply(0) == Money());
    REQUIRE(price == Money(1250));
    REQUIRE(fee == Money(150));
}

TEST_CASE("Money accepts exact numeric boundaries", "[money]") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    REQUIRE(Money(maximum).add(Money()) == Money(maximum));
    REQUIRE(Money(maximum - 1).add(Money(1)) == Money(maximum));
    REQUIRE(Money(maximum).multiply(1) == Money(maximum));
    REQUIRE(Money().multiply(maximum) == Money());
    REQUIRE(Money(maximum).subtract(Money(maximum)) == Money());
}

TEST_CASE("Money rejects negative construction and arithmetic", "[money]") {
    REQUIRE_THROWS_AS(Money(-1), InvalidAmountException);
    REQUIRE_THROWS_AS(Money(std::numeric_limits<std::int64_t>::min()), InvalidAmountException);
    const Money amount(100);
    REQUIRE_THROWS_AS(amount.subtract(Money(101)), InvalidAmountException);
    REQUIRE_THROWS_AS(amount.multiply(-1), InvalidAmountException);
    REQUIRE_THROWS_AS(Money().multiply(-1), InvalidAmountException);
    REQUIRE(amount == Money(100));
}

TEST_CASE("Money detects overflow before changing operands", "[money]") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const Money amount(maximum);
    const Money extra(1);
    REQUIRE_THROWS_AS(amount.add(extra), InvalidAmountException);
    REQUIRE_THROWS_AS(amount.multiply(2), InvalidAmountException);
    REQUIRE_THROWS_AS(Money(2).multiply(maximum), InvalidAmountException);
    REQUIRE(amount == Money(maximum));
    REQUIRE(extra == Money(1));
}

TEST_CASE("Money copies and domain exceptions remain usable", "[money]") {
    const Money original(100);
    Money copy = original;
    copy = copy.add(Money(25));
    REQUIRE(original == Money(100));
    REQUIRE(copy == Money(125));
    REQUIRE_THROWS_AS(Money(-1), post::PostalException);
    const InvalidAmountException error("Invalid test amount");
    REQUIRE(std::string(error.what()) == "Invalid test amount");
}
