#include "post/shipments/ContentItem.hpp"

#include "post/exceptions/InvalidAmountException.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"
#include "post/exceptions/PostalException.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using post::ContentCategory;
using post::ContentItem;
using post::Money;

TEST_CASE("ContentItem normalizes its name and calculates exact totals", "[content_item]") {
    const ContentItem item(" Учебные   книги ", 3, 250, Money{1234}, ContentCategory::GeneralGoods);
    REQUIRE(item.name() == "Учебные книги");
    REQUIRE(item.quantity() == 3);
    REQUIRE(item.unitWeightGrams() == 250);
    REQUIRE(item.unitDeclaredValue() == Money{1234});
    REQUIRE(item.category() == ContentCategory::GeneralGoods);
    REQUIRE(item.totalWeightGrams() == 750);
    REQUIRE(item.totalDeclaredValue() == Money{3702});
    STATIC_REQUIRE(std::is_same_v<decltype(item.name()), const std::string&>);
    STATIC_REQUIRE(std::is_same_v<decltype(item.unitDeclaredValue()), const Money&>);
}

TEST_CASE("ContentItem supports allowed categories and zero declared value", "[content_item]") {
    for (const auto category : {ContentCategory::Documents, ContentCategory::GeneralGoods,
                                ContentCategory::FragileGoods}) {
        const ContentItem item("Содержимое", 2, 1, Money{}, category);
        REQUIRE(item.category() == category);
        REQUIRE(item.totalWeightGrams() == 2);
        REQUIRE(item.totalDeclaredValue() == Money{});
    }
    for (const auto category : {ContentCategory::Prohibited, static_cast<ContentCategory>(-1),
                                static_cast<ContentCategory>(99)}) {
        REQUIRE_THROWS_AS(ContentItem("Содержимое", 1, 1, Money{}, category),
                          post::InvalidShipmentDataException);
    }
}

TEST_CASE("ContentItem rejects missing names and nonpositive quantities or weights",
          "[content_item]") {
    for (const auto& name : std::vector<std::string>{"", " \t ", "\x01", std::string("A\0B", 3)}) {
        REQUIRE_THROWS_AS(ContentItem(name, 1, 1, Money{}, ContentCategory::Documents),
                          post::InvalidShipmentDataException);
    }
    for (const std::int64_t value :
         {std::int64_t{0}, std::int64_t{-1}, std::numeric_limits<std::int64_t>::min()}) {
        REQUIRE_THROWS_AS(ContentItem("Документ", value, 1, Money{}, ContentCategory::Documents),
                          post::InvalidShipmentDataException);
        REQUIRE_THROWS_AS(ContentItem("Документ", 1, value, Money{}, ContentCategory::Documents),
                          post::InvalidShipmentDataException);
    }
    REQUIRE_THROWS_AS(ContentItem("", 1, 1, Money{}, ContentCategory::Documents),
                      post::PostalException);
}

TEST_CASE("ContentItem accepts exact integer boundaries", "[content_item]") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const ContentItem many("Документы", maximum, 1, Money{}, ContentCategory::Documents);
    REQUIRE(many.totalWeightGrams() == maximum);
    REQUIRE(many.totalDeclaredValue() == Money{});
    const ContentItem one("Груз", 1, maximum, Money{maximum}, ContentCategory::GeneralGoods);
    REQUIRE(one.totalWeightGrams() == maximum);
    REQUIRE(one.totalDeclaredValue() == Money{maximum});
    const ContentItem almost("Груз", 2, maximum / 2, Money{maximum / 2},
                             ContentCategory::GeneralGoods);
    REQUIRE(almost.totalWeightGrams() == maximum - 1);
    REQUIRE(almost.totalDeclaredValue() == Money{maximum - 1});
}

TEST_CASE("ContentItem detects overflow without changing its data", "[content_item]") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const ContentItem heavy("Груз", 2, maximum, Money{}, ContentCategory::GeneralGoods);
    const auto heavySnapshot = heavy;
    REQUIRE_THROWS_AS(heavy.totalWeightGrams(), std::overflow_error);
    REQUIRE(heavy == heavySnapshot);
    const ContentItem valuable("Груз", maximum, 1, Money{2}, ContentCategory::GeneralGoods);
    const auto valueSnapshot = valuable;
    REQUIRE_THROWS_AS(valuable.totalDeclaredValue(), post::InvalidAmountException);
    REQUIRE(valuable == valueSnapshot);
    REQUIRE(valuable.totalWeightGrams() == maximum);
}

TEST_CASE("ContentItem copies own data and failed replacement preserves the original",
          "[content_item]") {
    std::string name = "Книга";
    Money value{100};
    const ContentItem original(name, 2, 300, value, ContentCategory::GeneralGoods);
    name = "Чашка";
    value = Money{500};
    REQUIRE(original.name() == "Книга");
    REQUIRE(original.unitDeclaredValue() == Money{100});
    auto copy = original;
    REQUIRE(copy == original);
    REQUIRE_THROWS_AS(copy =
                          ContentItem("Книга", 0, 300, Money{100}, ContentCategory::GeneralGoods),
                      post::InvalidShipmentDataException);
    REQUIRE(copy == original);
    copy = ContentItem("Чашка", 1, 200, Money{500}, ContentCategory::FragileGoods);
    REQUIRE(copy != original);
    REQUIRE(original.totalWeightGrams() == 600);
    REQUIRE(original.totalDeclaredValue() == Money{200});
}
