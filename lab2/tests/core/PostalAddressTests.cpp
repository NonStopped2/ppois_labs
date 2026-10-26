#include "post/core/PostalAddress.hpp"
#include "post/exceptions/InvalidAddressException.hpp"
#include "post/exceptions/PostalException.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

using post::InvalidAddressException;
using post::PostalAddress;

TEST_CASE("PostalAddress normalizes spaces without damaging Cyrillic", "[address]") {
    const PostalAddress address(" 012345 ", " \tМинск\n ", "  улица   Лесная ", " 10А ", " 42 ");
    REQUIRE(address.postalCode() == "012345");
    REQUIRE(address.locality() == "Минск");
    REQUIRE(address.street() == "улица Лесная");
    REQUIRE(address.building() == "10А");
    REQUIRE(address.apartment() == std::optional<std::string>("42"));
    REQUIRE_NOTHROW(address.validate());
}

TEST_CASE("PostalAddress permits a missing apartment", "[address]") {
    const PostalAddress address("000001", "Минск", "Лесная", "1");
    REQUIRE_FALSE(address.apartment().has_value());
    REQUIRE_NOTHROW(address.validate());
}

TEST_CASE("PostalAddress requires exactly six ASCII digits", "[address]") {
    const std::vector<std::string> invalid{"",       "12345",  "1234567",     "12a456",
                                           "12 345", "+12345", "１２３４５６"};
    for (const auto& index : invalid) {
        CAPTURE(index);
        REQUIRE_THROWS_AS(PostalAddress(index, "Минск", "Лесная", "1"), InvalidAddressException);
    }
    REQUIRE_THROWS_AS(PostalAddress("bad", "Минск", "Лесная", "1"), post::PostalException);
}

TEST_CASE("PostalAddress rejects empty and control-character components", "[address]") {
    const std::vector<std::string> invalid{"", " \t\r\n\f\v ", std::string("a\0b", 3), "\x7f",
                                           "\x01"};
    for (const auto& part : invalid) {
        CAPTURE(part);
        REQUIRE_THROWS_AS(PostalAddress("220000", part, "Лесная", "1"), InvalidAddressException);
        REQUIRE_THROWS_AS(PostalAddress("220000", "Минск", part, "1"), InvalidAddressException);
        REQUIRE_THROWS_AS(PostalAddress("220000", "Минск", "Лесная", part),
                          InvalidAddressException);
        REQUIRE_THROWS_AS(PostalAddress("220000", "Минск", "Лесная", "1", part),
                          InvalidAddressException);
    }
}

TEST_CASE("PostalAddress compares every delivery component", "[address]") {
    const PostalAddress address("220000", "Минск", "Лесная", "1", "2");
    REQUIRE(address.sameDeliveryPointAs(PostalAddress("220000", " Минск ", "Лесная", "1", "2")));
    REQUIRE_FALSE(
        address.sameDeliveryPointAs(PostalAddress("220001", "Минск", "Лесная", "1", "2")));
    REQUIRE_FALSE(
        address.sameDeliveryPointAs(PostalAddress("220000", "Брест", "Лесная", "1", "2")));
    REQUIRE_FALSE(
        address.sameDeliveryPointAs(PostalAddress("220000", "Минск", "Полевая", "1", "2")));
    REQUIRE_FALSE(
        address.sameDeliveryPointAs(PostalAddress("220000", "Минск", "Лесная", "3", "2")));
    REQUIRE_FALSE(
        address.sameDeliveryPointAs(PostalAddress("220000", "Минск", "Лесная", "1", "3")));
    REQUIRE_FALSE(address.sameDeliveryPointAs(PostalAddress("220000", "Минск", "Лесная", "1")));
}

TEST_CASE("PostalAddress copies and failed replacement preserve the original", "[address]") {
    const PostalAddress original("220000", "Минск", "Лесная", "1", "2");
    auto copy = original;
    REQUIRE(copy == original);
    REQUIRE_THROWS_AS(copy = PostalAddress("bad", "Минск", "Лесная", "1"), InvalidAddressException);
    REQUIRE(copy == original);
    copy = PostalAddress("224000", "Брест", "Полевая", "3");
    REQUIRE(copy != original);
    REQUIRE(original.apartment() == std::optional<std::string>("2"));
    const InvalidAddressException error("Invalid test address");
    REQUIRE(std::string(error.what()) == "Invalid test address");
}
