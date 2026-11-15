#include "post/core/ContactDetails.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using post::ContactChannel;
using post::ContactDetails;

TEST_CASE("ContactDetails normalize phone separators and surrounding email spaces", "[contacts]") {
    const ContactDetails contacts(" +375 (29) 123-45-67 ", " user@example.by ");
    REQUIRE(contacts.phone() == "+375291234567");
    REQUIRE(contacts.email() == std::optional<std::string>("user@example.by"));
    REQUIRE(contacts.preferredChannel() == ContactChannel::Phone);
    REQUIRE_NOTHROW(contacts.validate());
}

TEST_CASE("ContactDetails accept supported phone boundaries", "[contacts]") {
    REQUIRE(ContactDetails("+1234567").phone() == "+1234567");
    REQUIRE(ContactDetails("+123456789012345").phone() == "+123456789012345");
    REQUIRE_FALSE(ContactDetails("+375291234567").email().has_value());
}

TEST_CASE("ContactDetails reject invalid phones", "[contacts]") {
    const std::vector<std::string> phones{
        "",         "() - ",        "+123456",      "+1234567890123456", "375291234567",
        "+0123456", "+37529abc567", "+37529+34567", "+37529/34567"};
    for (const auto& phone : phones) {
        CAPTURE(phone);
        REQUIRE_THROWS_AS(ContactDetails(phone), std::invalid_argument);
    }
}

TEST_CASE("ContactDetails accept supported email forms", "[contacts]") {
    const std::vector<std::string> emails{"user@example.by", "A1._%+-z@a1.mail-host.by", "1@a.b"};
    for (const auto& email : emails) {
        CAPTURE(email);
        REQUIRE_NOTHROW(ContactDetails("+375291234567", email, ContactChannel::Email));
    }
}

TEST_CASE("ContactDetails reject malformed emails", "[contacts]") {
    const std::vector<std::string> emails{"",
                                          "user.example.by",
                                          "user@@example.by",
                                          "@example.by",
                                          ".user@example.by",
                                          "user.@example.by",
                                          "us..er@example.by",
                                          "us&er@example.by",
                                          "us er@example.by",
                                          "user@",
                                          "user@.example.by",
                                          "user@example.by.",
                                          "user@-example.by",
                                          "user@example.by-",
                                          "user@localhost",
                                          "user@ex..ample.by",
                                          "user@a.-b.by",
                                          "user@a-.b.by",
                                          "user@exam_ple.by",
                                          "user@пример.бел",
                                          "user@example by"};
    for (const auto& email : emails) {
        CAPTURE(email);
        REQUIRE_THROWS_AS(ContactDetails("+375291234567", email), std::invalid_argument);
    }
}

TEST_CASE("ContactDetails select the preferred or first supported available channel",
          "[contacts]") {
    const ContactDetails phoneFirst("+375291234567", "user@example.by");
    const ContactDetails emailFirst("+375291234567", "user@example.by", ContactChannel::Email);
    REQUIRE(phoneFirst.selectChannel({ContactChannel::Email, ContactChannel::Phone}) ==
            ContactChannel::Phone);
    REQUIRE(emailFirst.selectChannel({ContactChannel::Phone, ContactChannel::Email}) ==
            ContactChannel::Email);
    REQUIRE(phoneFirst.selectChannel({ContactChannel::Email}) == ContactChannel::Email);
    REQUIRE(emailFirst.selectChannel({ContactChannel::Phone}) == ContactChannel::Phone);
}

TEST_CASE("ContactDetails reject unavailable and unknown channels", "[contacts]") {
    const ContactDetails contacts("+375291234567");
    const auto unknown = static_cast<ContactChannel>(99);
    REQUIRE_THROWS_AS(ContactDetails("+375291234567", std::nullopt, ContactChannel::Email),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(ContactDetails("+375291234567", std::nullopt, unknown),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(contacts.selectChannel({}), std::invalid_argument);
    REQUIRE_THROWS_AS(contacts.selectChannel({ContactChannel::Email}), std::invalid_argument);
    REQUIRE_THROWS_AS(contacts.selectChannel({unknown}), std::invalid_argument);
    REQUIRE_THROWS_AS(contacts.selectChannel({ContactChannel::Phone, unknown}),
                      std::invalid_argument);
}

TEST_CASE("ContactDetails copies and failed replacement preserve original values", "[contacts]") {
    const ContactDetails original("+375291234567", "user@example.by");
    auto copy = original;
    REQUIRE(copy == original);
    REQUIRE_THROWS_AS(copy = ContactDetails("bad"), std::invalid_argument);
    REQUIRE(copy == original);
    copy = ContactDetails("+375331234567");
    REQUIRE(copy != original);
    REQUIRE(original.email() == std::optional<std::string>("user@example.by"));
}
