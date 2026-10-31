#include "post/core/IdentityDocument.hpp"

#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/PostalException.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using post::DocumentKind;
using post::IdentityDocument;
using namespace std::chrono;

TEST_CASE("IdentityDocument normalizes data and exposes read-only fields", "[identity_document]") {
    const IdentityDocument document(DocumentKind::Passport, " AB1234567 ", " Васильев   Арсений ",
                                    2020y / January / 10, 2030y / January / 10);
    REQUIRE(document.kind() == DocumentKind::Passport);
    REQUIRE(document.number() == "AB1234567");
    REQUIRE(document.holderName() == "Васильев Арсений");
    REQUIRE(document.issuedOn() == 2020y / January / 10);
    REQUIRE(document.expiresOn() == 2030y / January / 10);
    STATIC_REQUIRE(std::is_same_v<decltype(document.number()), const std::string&>);
    STATIC_REQUIRE(std::is_same_v<decltype(document.holderName()), const std::string&>);
}

TEST_CASE("IdentityDocument supports both kinds and inclusive validity dates",
          "[identity_document]") {
    for (const auto kind : {DocumentKind::Passport, DocumentKind::IdentityCard}) {
        const IdentityDocument document(kind, "ID123", "Арсений", 2024y / February / 29,
                                        2025y / March / 1);
        REQUIRE(document.kind() == kind);
        REQUIRE_FALSE(document.isValidOn(2024y / February / 28));
        REQUIRE(document.isValidOn(2024y / February / 29));
        REQUIRE(document.isValidOn(2024y / December / 31));
        REQUIRE(document.isValidOn(2025y / March / 1));
        REQUIRE_FALSE(document.isValidOn(2025y / March / 2));
    }
    const IdentityDocument oneDay(DocumentKind::IdentityCard, "ID1", "Арсений", 2024y / June / 1,
                                  2024y / June / 1);
    REQUIRE(oneDay.isValidOn(2024y / June / 1));
}

TEST_CASE("IdentityDocument rejects unsupported kinds and invalid text", "[identity_document]") {
    for (const int value : {-1, 2, 99}) {
        REQUIRE_THROWS_AS(IdentityDocument(static_cast<DocumentKind>(value), "ID1", "Арсений",
                                           2020y / January / 1, 2030y / January / 1),
                          std::invalid_argument);
    }
    const std::vector<std::string> numbers{"",        " \t ", "AB 123",
                                           "AB\n123", "\x01", std::string("A\0B", 3)};
    for (const auto& number : numbers) {
        CAPTURE(number);
        REQUIRE_THROWS_AS(IdentityDocument(DocumentKind::Passport, number, "Арсений",
                                           2020y / January / 1, 2030y / January / 1),
                          std::invalid_argument);
    }
    for (const auto& name : std::vector<std::string>{"", " \n ", "\x7f", std::string("A\0B", 3)}) {
        REQUIRE_THROWS_AS(IdentityDocument(DocumentKind::Passport, "ID1", name, 2020y / January / 1,
                                           2030y / January / 1),
                          std::invalid_argument);
    }
}

TEST_CASE("IdentityDocument rejects impossible and reversed calendar periods",
          "[identity_document]") {
    const auto start = 2020y / January / 1;
    const auto end = 2030y / January / 1;
    const std::array invalidDates{2023y / February / 29, 2024y / April / 31,
                                  2024y / month{0} / day{1}, 2024y / January / day{0}};
    for (const auto date : invalidDates) {
        REQUIRE_THROWS_AS(IdentityDocument(DocumentKind::Passport, "ID1", "Арсений", date, end),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(IdentityDocument(DocumentKind::Passport, "ID1", "Арсений", start, date),
                          std::invalid_argument);
    }
    REQUIRE_THROWS_AS(IdentityDocument(DocumentKind::Passport, "ID1", "Арсений", end, start),
                      std::invalid_argument);
    const IdentityDocument document(DocumentKind::Passport, "ID1", "Арсений", start, end);
    REQUIRE_THROWS_AS(document.isValidOn(2024y / February / 30), std::invalid_argument);
    REQUIRE_THROWS_AS(document.validateForHolder("Арсений", 2024y / February / 30),
                      std::invalid_argument);
}

TEST_CASE("IdentityDocument authorizes only its named holder within the period",
          "[identity_document]") {
    const IdentityDocument document(DocumentKind::Passport, "ID1", "Васильев Арсений",
                                    2020y / January / 1, 2030y / January / 1);
    REQUIRE_NOTHROW(document.validateForHolder(" Васильев\tАрсений ", 2025y / June / 1));
    REQUIRE_NOTHROW(document.validateForHolder("Васильев Арсений", document.issuedOn()));
    REQUIRE_NOTHROW(document.validateForHolder("Васильев Арсений", document.expiresOn()));
    for (const auto& name : {"", "Другой человек", "васильев Арсений"}) {
        REQUIRE_THROWS_AS(document.validateForHolder(name, 2025y / June / 1),
                          post::DeliveryAuthorizationException);
    }
    for (const auto date : {2019y / December / 31, 2030y / January / 2}) {
        REQUIRE_THROWS_AS(document.validateForHolder("Васильев Арсений", date),
                          post::DeliveryAuthorizationException);
    }
    REQUIRE_THROWS_AS(document.validateForHolder("Другой", 2025y / June / 1),
                      post::PostalException);
}

TEST_CASE("IdentityDocument copies own data and failed replacement preserves the original",
          "[identity_document]") {
    std::string number = "ID1";
    std::string name = "Арсений";
    const IdentityDocument original(DocumentKind::Passport, number, name, 2020y / January / 1,
                                    2030y / January / 1);
    number = "OTHER";
    name = "Иван";
    REQUIRE(original.number() == "ID1");
    REQUIRE(original.holderName() == "Арсений");
    auto copy = original;
    REQUIRE(copy == original);
    REQUIRE_THROWS_AS(copy = IdentityDocument(DocumentKind::Passport, "", "Арсений",
                                              2020y / January / 1, 2030y / January / 1),
                      std::invalid_argument);
    REQUIRE(copy == original);
    REQUIRE_THROWS_AS(copy.validateForHolder("Иван", 2025y / June / 1),
                      post::DeliveryAuthorizationException);
    REQUIRE(copy == original);
    copy = IdentityDocument(DocumentKind::IdentityCard, "ID2", "Иван", 2021y / January / 1,
                            2031y / January / 1);
    REQUIRE(copy != original);
    REQUIRE(original.number() == "ID1");
}
