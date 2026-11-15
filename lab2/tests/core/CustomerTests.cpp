#include "post/core/Customer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using post::ContactDetails;
using post::Customer;
using post::PostalAddress;

TEST_CASE("Customer normalizes identifying data and exposes read-only values", "[customer]") {
    const PostalAddress address("220000", "Минск", "Лесная", "1");
    const ContactDetails contacts("+375291234567");
    const Customer customer(" client-1 ", " Васильев   Арсений ", address, contacts);
    REQUIRE(customer.id() == "client-1");
    REQUIRE(customer.fullName() == "Васильев Арсений");
    REQUIRE(customer.address() == address);
    REQUIRE(customer.contacts() == contacts);
    STATIC_REQUIRE(std::is_same_v<decltype(customer.address()), const PostalAddress&>);
    STATIC_REQUIRE(std::is_same_v<decltype(customer.contacts()), const ContactDetails&>);
}

TEST_CASE("Customer rejects missing identifiers and names", "[customer]") {
    const PostalAddress address("220000", "Минск", "Лесная", "1");
    const ContactDetails contacts("+375291234567");
    const std::vector<std::string> invalidIds{"", " \t ", "client 1", "client\t1", "\x01"};
    for (const auto& id : invalidIds) {
        CAPTURE(id);
        REQUIRE_THROWS_AS(Customer(id, "Арсений", address, contacts), std::invalid_argument);
    }
    const std::vector<std::string> invalidNames{"", " \n ", std::string("a\0b", 3)};
    for (const auto& name : invalidNames) {
        CAPTURE(name);
        REQUIRE_THROWS_AS(Customer("client-1", name, address, contacts), std::invalid_argument);
    }
}

TEST_CASE("Customer owns snapshots rather than caller references", "[customer]") {
    PostalAddress address("220000", "Минск", "Лесная", "1", "2");
    ContactDetails contacts("+375291234567", "user@example.by");
    std::string name = "Арсений";
    const Customer customer("client-1", name, address, contacts);
    address = PostalAddress("224000", "Брест", "Полевая", "3");
    contacts = ContactDetails("+375331234567");
    name = "Другое имя";
    REQUIRE(customer.fullName() == "Арсений");
    REQUIRE(customer.address().locality() == "Минск");
    REQUIRE(customer.address().apartment() == std::optional<std::string>("2"));
    REQUIRE(customer.contacts().phone() == "+375291234567");
}

TEST_CASE("Customer copies and failed replacement preserve all fields", "[customer]") {
    const PostalAddress address("220000", "Минск", "Лесная", "1");
    const ContactDetails contacts("+375291234567");
    const Customer original("client-1", "Арсений", address, contacts);
    auto copy = original;
    REQUIRE(copy == original);
    REQUIRE_THROWS_AS(copy = Customer("", "Арсений", address, contacts), std::invalid_argument);
    REQUIRE(copy == original);
    copy = Customer("client-2", "Иван", PostalAddress("224000", "Брест", "Полевая", "2"),
                    ContactDetails("+375331234567"));
    REQUIRE(copy != original);
    REQUIRE(original.id() == "client-1");
    REQUIRE(original.address() == address);
    REQUIRE(original.contacts() == contacts);
}
