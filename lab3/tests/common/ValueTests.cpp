#include "shop/common/Address.hpp"
#include "shop/common/ContactDetails.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/common/Money.hpp"
#include "shop/common/TimeWindow.hpp"
#include "shop/common/Validation.hpp"

#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace shop;

TEST_CASE("Money checks signed arithmetic and formats boundary values") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto minimum = std::numeric_limits<std::int64_t>::min();
    REQUIRE(Money(100).checkedAdd(Money(23)) == Money(123));
    REQUIRE(Money(-100).checkedSubtract(Money(23)) == Money(-123));
    REQUIRE(Money(0).checkedSubtract(Money(minimum + 1)) == Money(maximum));
    REQUIRE_THROWS_AS(Money(maximum).checkedAdd(Money(1)), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Money(minimum).checkedAdd(Money(-1)), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Money(maximum).checkedSubtract(Money(-1)), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Money(minimum).checkedSubtract(Money(1)), InvalidDomainDataException);
    REQUIRE(Money(-3).multiply(2) == Money(-6));
    REQUIRE(Money(minimum).multiply(1) == Money(minimum));
    REQUIRE(Money(minimum).multiply(0) == Money{});
    REQUIRE_THROWS_AS(Money(maximum).multiply(2), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Money(minimum).multiply(2), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Money(1).multiply(-1), InvalidDomainDataException);
    REQUIRE(Money(3).format() == "0.03 BYN");
    REQUIRE(Money(-123).format() == "-1.23 BYN");
    REQUIRE(Money(maximum).format() == "92233720368547758.07 BYN");
    REQUIRE(Money(minimum).format() == "-92233720368547758.08 BYN");
}

TEST_CASE("Money fractions agree with independent small-integer products") {
    for (std::int64_t amount = 0; amount <= 100; ++amount) {
        for (std::int64_t divisor = 1; divisor <= 20; ++divisor) {
            for (std::int64_t numerator = 0; numerator <= divisor; ++numerator) {
                REQUIRE(Money(amount).fractionFloor(numerator, divisor).kopecks() ==
                        amount * numerator / divisor);
                REQUIRE(Money(amount).fractionParts(numerator, divisor).second ==
                        amount * numerator % divisor);
            }
        }
    }
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    REQUIRE(Money(maximum).fractionFloor(maximum, maximum) == Money(maximum));
    REQUIRE(Money(maximum).fractionFloor(maximum - 1, maximum) == Money(maximum - 1));
    REQUIRE(Money(199).percentageFloor(5000) == Money(99));
    REQUIRE_THROWS_AS(Money(-1).percentageFloor(1), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Money(1).fractionFloor(0, 0), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Money(1).fractionFloor(-1, 2), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Money(1).percentageFloor(10001), InvalidDomainDataException);
}

TEST_CASE("Address owns validated fields and preserves leading postal zeroes") {
    const Address address("BY", "012345", " Минск ", "Лесная", "1", "");
    REQUIRE(address.country() == "BY");
    REQUIRE(address.postalCode() == "012345");
    REQUIRE(address.city() == "Минск");
    REQUIRE(address.street() == "Лесная");
    REQUIRE(address.building() == "1");
    REQUIRE(address.apartment().empty());
    REQUIRE(Address(address) == address);
    REQUIRE_THROWS_AS(Address("US", "012345", "City", "Street", "1"), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Address("BY", "12345x", "City", "Street", "1"), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Address("BY", "12345", "City", "Street", "1"), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Address("BY", "123456", " ", "Street", "1"), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Address("BY", "123456", "City", "a\nb", "1"), InvalidDomainDataException);
}

TEST_CASE("Contacts normalize phone numbers but do not pretend to verify identities") {
    const ContactDetails contacts(" Анна ", "anna@example.org", "+375 (29) 123-45-67");
    REQUIRE(contacts.fullName() == "Анна");
    REQUIRE(contacts.email() == "anna@example.org");
    REQUIRE(contacts.phone() == "+375291234567");
    REQUIRE(ContactDetails(contacts) == contacts);
    REQUIRE(ContactDetails("Name", "a@b", "1234567").phone() == "1234567");
    for (const auto* email : {"a", "@b", "a@", "a@b@c", "a b@c"}) {
        REQUIRE_THROWS_AS(ContactDetails("Name", email, "1234567"), InvalidDomainDataException);
    }
    for (const auto* phone : {"123", "+", "++1234567", "123456a", "1234567890123456"}) {
        REQUIRE_THROWS_AS(ContactDetails("Name", "a@b", phone), InvalidDomainDataException);
    }
    REQUIRE_THROWS_AS(detail::text(std::string(1025, 'x'), "name"), InvalidDomainDataException);
}

TEST_CASE("Time intervals are half-open and clock failures preserve its value") {
    const TimeWindow window(10, 20);
    REQUIRE(window.startsAt() == 10);
    REQUIRE(window.endsAt() == 20);
    REQUIRE_FALSE(window.contains(9));
    REQUIRE(window.contains(10));
    REQUIRE_FALSE(window.contains(20));
    REQUIRE(window.overlaps(TimeWindow(19, 21)));
    REQUIRE_FALSE(window.overlaps(TimeWindow(20, 30)));
    REQUIRE_THROWS_AS(TimeWindow(1, 1), InvalidDomainDataException);
    REQUIRE_THROWS_AS(TimeWindow(-1, 1), InvalidDomainDataException);
    ManualClock clock(10);
    clock.advance(20);
    REQUIRE(clock.now() == 30);
    REQUIRE_THROWS_AS(clock.advance(-1), InvalidDomainDataException);
    REQUIRE(clock.now() == 30);
    ManualClock boundary(std::numeric_limits<std::int64_t>::max());
    REQUIRE_THROWS_AS(boundary.advance(1), InvalidDomainDataException);
    REQUIRE(boundary.now() == std::numeric_limits<std::int64_t>::max());
    REQUIRE_THROWS_AS(ManualClock(-1), InvalidDomainDataException);
}
