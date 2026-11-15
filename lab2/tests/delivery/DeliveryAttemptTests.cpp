#include "../BranchFixtures.hpp"
#include "post/delivery/DeliveryAttempt.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("DeliveryAttempt validates the full result reason matrix and owns its data",
          "[delivery-attempt]") {
    for (int result = 0; result < 2; ++result) {
        for (int reason = 0; reason < 5; ++reason) {
            const bool valid = (result == 0) == (reason == 0);
            if (valid) {
                REQUIRE_NOTHROW(
                    DeliveryAttempt("a", test::moment(10), static_cast<DeliveryOutcome>(result),
                                    static_cast<DeliveryFailureReason>(reason), "Пояснение"));
            } else {
                REQUIRE_THROWS_AS(
                    DeliveryAttempt("a", test::moment(10), static_cast<DeliveryOutcome>(result),
                                    static_cast<DeliveryFailureReason>(reason), "Пояснение"),
                    InvalidShipmentDataException);
            }
        }
    }
    const DeliveryAttempt success(" a ", test::moment(10), DeliveryOutcome::Delivered);
    REQUIRE(success.id() == "a");
    REQUIRE(success.attemptedAt() == test::moment(10));
    REQUIRE(success.outcome() == DeliveryOutcome::Delivered);
    REQUIRE(success.failureReason() == DeliveryFailureReason::None);
    REQUIRE(success.note().empty());
    REQUIRE_NOTHROW(success.validateOutcome());
    REQUIRE(success == success);
    REQUIRE_THROWS_AS(DeliveryAttempt("a", test::moment(10), static_cast<DeliveryOutcome>(99)),
                      InvalidShipmentDataException);
    REQUIRE_THROWS_AS(DeliveryAttempt("a", test::moment(10), DeliveryOutcome::Failed,
                                      static_cast<DeliveryFailureReason>(99), "Причина"),
                      InvalidShipmentDataException);
    REQUIRE_THROWS_AS(DeliveryAttempt("a", test::moment(10), DeliveryOutcome::Failed,
                                      DeliveryFailureReason::RecipientAbsent),
                      InvalidShipmentDataException);
}
TEST_CASE("DeliveryAttempt permits only retryable failure after inclusive interval",
          "[delivery-attempt]") {
    for (const auto reason :
         {DeliveryFailureReason::RecipientAbsent, DeliveryFailureReason::AddressNotFound,
          DeliveryFailureReason::Refused, DeliveryFailureReason::Damaged}) {
        const DeliveryAttempt attempt("a", test::moment(10), DeliveryOutcome::Failed, reason,
                                      " Неудача ");
        REQUIRE(attempt.note() == "Неудача");
        const bool retryable = reason == DeliveryFailureReason::RecipientAbsent ||
                               reason == DeliveryFailureReason::AddressNotFound;
        REQUIRE_FALSE(attempt.canRetryAfter(test::moment(19), std::chrono::seconds{10}));
        REQUIRE(attempt.canRetryAfter(test::moment(20), std::chrono::seconds{10}) == retryable);
        REQUIRE(attempt.canRetryAfter(test::moment(10), std::chrono::seconds{0}) == retryable);
    }
    const DeliveryAttempt attempt("a", test::moment(10), DeliveryOutcome::Delivered);
    REQUIRE_FALSE(attempt.canRetryAfter(test::moment(100), std::chrono::seconds{0}));
    REQUIRE_THROWS_AS(attempt.canRetryAfter(test::moment(9), std::chrono::seconds{0}),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(attempt.canRetryAfter(test::moment(10), std::chrono::seconds{-1}),
                      std::invalid_argument);
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const DeliveryAttempt edge("e", test::moment(maximum - 1), DeliveryOutcome::Failed,
                               DeliveryFailureReason::RecipientAbsent, "Нет дома");
    REQUIRE(edge.canRetryAfter(test::moment(maximum), std::chrono::seconds{1}));
    REQUIRE_FALSE(edge.canRetryAfter(test::moment(maximum), std::chrono::seconds{maximum}));
}
