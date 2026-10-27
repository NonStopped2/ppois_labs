#include "../TariffFixtures.hpp"
#include "post/exceptions/InvalidAmountException.hpp"
#include "post/storage/StoragePolicy.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace post;
namespace {
constexpr std::int64_t kDay = 86400;
constexpr std::int64_t kHour = 3600;
} // namespace
TEST_CASE("StoragePolicy exposes explicit configuration and an exact deadline",
          "[storage-policy]") {
    const StoragePolicy policy(" storage ", 3, 5, Money(20), 3, 2);
    REQUIRE(policy.id() == "storage");
    REQUIRE(policy.freeStorageDays() == 3);
    REQUIRE(policy.maxStorageDays() == 5);
    REQUIRE(policy.dailyFee() == Money(20));
    REQUIRE(policy.maxDeliveryAttempts() == 3);
    REQUIRE(policy.minRetryHours() == 2);
    REQUIRE(policy.deadlineFrom(test::moment(100)) == test::moment(100 + 5 * kDay));
    auto copy = policy;
    REQUIRE(copy == policy);
    copy = StoragePolicy("other", 0, 1, Money{}, 1, 0);
    REQUIRE(copy != policy);
}
TEST_CASE("StoragePolicy charges started paid days and caps accrual", "[storage-policy]") {
    const StoragePolicy policy("p", 3, 5, Money(20), 3, 2);
    for (std::int64_t hour = 0; hour <= 7 * 24; ++hour) {
        for (const auto offset : {0, 1}) {
            const auto age = hour * kHour + offset;
            const auto startedDays = (age + kDay - 1) / kDay;
            const auto expectedDays =
                std::max<std::int64_t>(0, std::min<std::int64_t>(startedDays, 5) - 3);
            REQUIRE(policy.chargeUntil(test::moment(100), test::moment(100 + age)) ==
                    Money(expectedDays * 20));
        }
    }
    REQUIRE(policy.chargeUntil(test::moment(0), test::moment(3 * kDay)) == Money{});
    REQUIRE(policy.chargeUntil(test::moment(0), test::moment(3 * kDay + 1)) == Money(20));
    REQUIRE(policy.chargeUntil(test::moment(0), test::moment(5 * kDay + 1)) == Money(40));
    REQUIRE(StoragePolicy("free", 5, 5, Money(20), 1, 0)
                .chargeUntil(test::moment(0), test::moment(6 * kDay)) == Money{});
}
TEST_CASE("StoragePolicy delivery eligibility checks limit, retry and expiry boundaries",
          "[storage-policy]") {
    const StoragePolicy policy("p", 0, 1, Money{}, 3, 2);
    REQUIRE(policy.canAttemptDelivery(test::moment(0), test::moment(0), 0));
    REQUIRE_FALSE(policy.canAttemptDelivery(test::moment(0), test::moment(2 * kHour - 1), 1,
                                            test::moment(0)));
    REQUIRE(
        policy.canAttemptDelivery(test::moment(0), test::moment(2 * kHour), 1, test::moment(0)));
    REQUIRE(
        policy.canAttemptDelivery(test::moment(0), test::moment(2 * kHour), 2, test::moment(0)));
    REQUIRE_FALSE(
        policy.canAttemptDelivery(test::moment(0), test::moment(2 * kHour), 3, test::moment(0)));
    REQUIRE(policy.canAttemptDelivery(test::moment(0), test::moment(kDay - 1), 0));
    REQUIRE_FALSE(policy.canAttemptDelivery(test::moment(0), test::moment(kDay), 0));
    REQUIRE(StoragePolicy("immediate", 0, 1, Money{}, 2, 0)
                .canAttemptDelivery(test::moment(0), test::moment(0), 1, test::moment(0)));
}
TEST_CASE("StoragePolicy rejects inconsistent observations without changing configuration",
          "[storage-policy]") {
    const StoragePolicy policy("p", 0, 1, Money(1), 3, 2);
    const auto before = policy;
    REQUIRE_THROWS_AS(policy.chargeUntil(test::moment(10), test::moment(9)), std::invalid_argument);
    REQUIRE_THROWS_AS(policy.chargeUntil(test::moment(-1), test::moment(0)), std::invalid_argument);
    REQUIRE_THROWS_AS(policy.canAttemptDelivery(test::moment(0), test::moment(0), -1),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(policy.canAttemptDelivery(test::moment(0), test::moment(0), 1),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        policy.canAttemptDelivery(test::moment(0), test::moment(0), 0, test::moment(0)),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        policy.canAttemptDelivery(test::moment(10), test::moment(20), 1, test::moment(9)),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        policy.canAttemptDelivery(test::moment(10), test::moment(20), 1, test::moment(21)),
        std::invalid_argument);
    REQUIRE(policy == before);
}
TEST_CASE("StoragePolicy rejects invalid settings and handles representation limits",
          "[storage-policy]") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    REQUIRE_THROWS_AS(StoragePolicy("p", -1, 1, Money{}, 1, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(StoragePolicy("p", 0, 0, Money{}, 1, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(StoragePolicy("p", 2, 1, Money{}, 1, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(StoragePolicy("p", 0, 1, Money{}, 0, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(StoragePolicy("p", 0, 1, Money{}, 1, -1), std::invalid_argument);
    REQUIRE_THROWS_AS(StoragePolicy("p", 0, maximum, Money{}, 1, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(StoragePolicy("p", 0, 1, Money{}, 1, maximum), std::invalid_argument);
    const StoragePolicy policy("p", 0, 2, Money(maximum), 1, 0);
    REQUIRE(policy.deadlineFrom(test::moment(maximum - 2 * kDay)) == test::moment(maximum));
    REQUIRE_THROWS_AS(policy.deadlineFrom(test::moment(maximum - 2 * kDay + 1)),
                      std::overflow_error);
    REQUIRE_THROWS_AS(policy.chargeUntil(test::moment(0), test::moment(2 * kDay)),
                      InvalidAmountException);
    REQUIRE(policy.chargeUntil(test::moment(maximum), test::moment(maximum)) == Money{});
}
