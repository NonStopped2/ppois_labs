#include "../Fixtures.hpp"
#include "post/exceptions/IncompatibleServiceException.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;

TEST_CASE("Letter preserves its product-specific data and accepts exact limits", "[letter]") {
    const auto letter = test::letter();
    REQUIRE(letter.kind() == ShipmentKind::Letter);
    REQUIRE(letter.envelope() == Dimensions(5, 200, 100));
    REQUIRE(letter.dimensions() == letter.envelope());
    REQUIRE(letter.sheetCount() == 2);
    REQUIRE(letter.contentsDescription() == "Документы");
    REQUIRE_NOTHROW(letter.validateForPosting(20, Dimensions(200, 100, 5), 2));
    auto copy = letter.clone();
    REQUIRE(dynamic_cast<Letter*>(copy.get()) != nullptr);
    copy->revise(30, DeliveryOptions{});
    REQUIRE(letter.weightGrams() == 20);
}
TEST_CASE("Letter checks every product limit and bad limit configuration", "[letter]") {
    const auto letter = test::letter();
    REQUIRE_THROWS_AS(letter.validateForPosting(19, Dimensions(200, 100, 5), 2),
                      ShipmentLimitExceededException);
    REQUIRE_THROWS_AS(letter.validateForPosting(20, Dimensions(200, 100, 4), 2),
                      ShipmentLimitExceededException);
    REQUIRE_THROWS_AS(letter.validateForPosting(20, Dimensions(200, 100, 5), 1),
                      ShipmentLimitExceededException);
    REQUIRE_THROWS_AS(letter.validateForPosting(0, Dimensions(200, 100, 5), 2),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(letter.validateForPosting(20, Dimensions(200, 100, 5), 0),
                      std::invalid_argument);
}
TEST_CASE("Letter rejects invalid sheets, weight, description and parcel-only services",
          "[letter]") {
    const auto person = test::customer();
    for (const auto count : {0, -1}) {
        REQUIRE_THROWS_AS(
            Letter(person, person, 1, DeliveryOptions{}, Dimensions(1, 1, 1), count, "x"),
            InvalidShipmentDataException);
    }
    REQUIRE_THROWS_AS(Letter(person, person, 0, DeliveryOptions{}, Dimensions(1, 1, 1), 1, "x"),
                      InvalidShipmentDataException);
    REQUIRE_THROWS_AS(Letter(person, person, 1, DeliveryOptions{}, Dimensions(1, 1, 1), 1, " "),
                      InvalidShipmentDataException);
    REQUIRE_THROWS_AS(Letter(person, person, 1, DeliveryOptions(false, Money{}, false, true),
                             Dimensions(1, 1, 1), 1, "x"),
                      IncompatibleServiceException);
}
