#include "../StaffFixtures.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("WorkShift owns schedule and records valid chronological work", "[work-shift]") {
    WorkShift shift(" s ", "worker", "A", test::moment(10), test::moment(100));
    REQUIRE(shift.id() == "s");
    REQUIRE(shift.employeeId() == "worker");
    REQUIRE(shift.branchId() == "A");
    REQUIRE(shift.scheduledStart() == test::moment(10));
    REQUIRE(shift.scheduledEnd() == test::moment(100));
    REQUIRE(shift.state() == ShiftState::Scheduled);
    REQUIRE_FALSE(shift.actualStart());
    REQUIRE_FALSE(shift.actualEnd());
    shift.start(test::employee(), "A", test::moment(20));
    shift.addBreak(test::moment(30), test::moment(40));
    shift.addBreak(test::moment(50), test::moment(60));
    REQUIRE(shift.isWorkingAt(test::moment(29)));
    REQUIRE_FALSE(shift.isWorkingAt(test::moment(30)));
    REQUIRE(shift.isWorkingAt(test::moment(40)));
    REQUIRE_FALSE(shift.isWorkingAt(test::moment(100)));
    REQUIRE(shift.actualStart() == test::moment(20));
    REQUIRE(shift.breaks().size() == 2);
    shift.finish(test::moment(90));
    REQUIRE(shift.actualEnd() == test::moment(90));
    REQUIRE(shift.state() == ShiftState::Finished);
    REQUIRE_FALSE(shift.isWorkingAt(test::moment(70)));
    for (int asOf = 20; asOf <= 110; ++asOf) {
        int worked = 0;
        for (int second = 20; second < std::min(asOf, 90); ++second) {
            if (!(second >= 30 && second < 40) && !(second >= 50 && second < 60)) {
                ++worked;
            }
        }
        REQUIRE(shift.workedDuration(test::moment(asOf)) == std::chrono::seconds{worked});
    }
}
TEST_CASE("WorkShift rejects invalid starts and repeated operations without changes",
          "[work-shift]") {
    WorkShift shift("s", "worker", "A", test::moment(10), test::moment(100));
    const auto before = shift;
    REQUIRE_THROWS_AS(shift.start(test::employee("other"), "A", test::moment(10)),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(shift.start(test::employee(), "B", test::moment(10)),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(shift.start(test::employee(), "A", test::moment(9)), std::invalid_argument);
    REQUIRE_THROWS_AS(shift.start(test::employee(), "A", test::moment(100)), std::invalid_argument);
    REQUIRE(shift == before);
    REQUIRE_THROWS_AS(shift.workedDuration(test::moment(10)), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(shift.addBreak(test::moment(10), test::moment(20)),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(shift.finish(test::moment(20)), InvalidShipmentStateException);
    shift.start(test::employee(), "A", test::moment(10));
    REQUIRE_THROWS_AS(shift.start(test::employee(), "A", test::moment(10)),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(shift.workedDuration(test::moment(9)), std::invalid_argument);
    shift.finish(test::moment(10));
    REQUIRE(shift.workedDuration(test::moment(20)) == std::chrono::seconds{0});
    REQUIRE_THROWS_AS(shift.finish(test::moment(20)), InvalidShipmentStateException);
}
TEST_CASE("WorkShift checks break boundaries touching intervals and copies", "[work-shift]") {
    auto shift = test::shift();
    for (const auto [from, to] : std::vector<std::pair<int, int>>{{10, 10}, {20, 10}, {0, 10001}}) {
        REQUIRE_THROWS_AS(shift.addBreak(test::moment(from), test::moment(to)),
                          std::invalid_argument);
    }
    shift.addBreak(test::moment(10), test::moment(20));
    shift.addBreak(test::moment(20), test::moment(30));
    const auto before = shift;
    REQUIRE_THROWS_AS(shift.addBreak(test::moment(29), test::moment(31)), std::invalid_argument);
    REQUIRE_THROWS_AS(shift.finish(test::moment(29)), std::invalid_argument);
    REQUIRE_THROWS_AS(shift.finish(test::moment(10001)), std::invalid_argument);
    REQUIRE(shift == before);
    auto copy = shift;
    copy.finish(test::moment(100));
    REQUIRE(copy != shift);
    REQUIRE(shift.state() == ShiftState::Active);
    REQUIRE(shift.workedDuration(test::moment(20000)) == std::chrono::seconds{9980});
}
TEST_CASE("WorkShift detects scheduling conflicts and rejects empty intervals", "[work-shift]") {
    const WorkShift first("a", "worker", "A", test::moment(10), test::moment(20));
    REQUIRE(first.overlaps(WorkShift("b", "worker", "B", test::moment(19), test::moment(30))));
    REQUIRE_FALSE(
        first.overlaps(WorkShift("b", "worker", "A", test::moment(20), test::moment(30))));
    REQUIRE_FALSE(first.overlaps(WorkShift("b", "other", "A", test::moment(10), test::moment(20))));
    REQUIRE_THROWS_AS(WorkShift("s", "w", "A", test::moment(20), test::moment(20)),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(WorkShift("s", "w", "A", test::moment(-1), test::moment(20)),
                      std::invalid_argument);
}
