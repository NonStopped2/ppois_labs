#include "post/tracking/TrackingHistory.hpp"

#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/PostalException.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using post::TrackingEvent;
using post::TrackingEventKind;
using post::TrackingHistory;
using namespace std::chrono;

namespace {
sys_seconds at(std::int64_t value) {
    return sys_seconds{seconds{value}};
}

TrackingEvent accepted(std::int64_t sequence, std::int64_t time, const std::string& branch = "A") {
    return TrackingEvent(sequence, TrackingEventKind::Accepted, at(time), branch);
}
} // namespace

TEST_CASE("TrackingHistory handles empty histories and read-only access", "[tracking_history]") {
    const TrackingHistory history;
    REQUIRE(history.events().empty());
    REQUIRE(history.eventsBetween(at(0), at(100)).empty());
    REQUIRE_FALSE(history.lastKnownBranch().has_value());
    REQUIRE(history.timeSpentAt("A", at(0)) == 0s);
    REQUIRE(history.timeSpentAt("A", at(100)) == 0s);
    STATIC_REQUIRE(std::is_same_v<decltype(history.events()), const std::vector<TrackingEvent>&>);
}

TEST_CASE("TrackingHistory accepts sparse sequences and equal timestamps", "[tracking_history]") {
    TrackingHistory history;
    history.append(accepted(1, 10));
    history.append(
        TrackingEvent(5, TrackingEventKind::Information, at(10), std::nullopt, "Сообщение"));
    history.append(TrackingEvent(9, TrackingEventKind::Dispatched, at(20), "A"));
    REQUIRE(history.events().size() == 3);
    REQUIRE(history.events()[1].sequence() == 5);
    REQUIRE(history.events()[1].occurredAt() == at(10));
    REQUIRE(history.lastKnownBranch() == std::optional<std::string>{"A"});
}

TEST_CASE("TrackingHistory rejects duplicates and disorder without partial changes",
          "[tracking_history]") {
    TrackingHistory history;
    history.append(accepted(10, 20));
    const auto snapshot = history;
    for (const auto& event : {accepted(10, 30), accepted(9, 30), accepted(11, 19)}) {
        REQUIRE_THROWS_AS(history.append(event), post::InvalidShipmentStateException);
        REQUIRE(history == snapshot);
    }
    REQUIRE_THROWS_AS(history.append(history.events().front()), post::PostalException);
    REQUIRE(history == snapshot);
    REQUIRE_NOTHROW(history.append(accepted(11, 20, "B")));
    REQUIRE(history.events().size() == 2);
}

TEST_CASE("TrackingHistory selects inclusive intervals and returns independent values",
          "[tracking_history]") {
    TrackingHistory history;
    const auto first = accepted(1, 10);
    const TrackingEvent second(2, TrackingEventKind::Information, at(10), std::nullopt,
                               "Сообщение");
    const TrackingEvent third(3, TrackingEventKind::Dispatched, at(20), "A");
    history.append(first);
    history.append(second);
    history.append(third);
    REQUIRE(history.eventsBetween(at(10), at(10)) == std::vector{first, second});
    REQUIRE(history.eventsBetween(at(10), at(20)) == std::vector{first, second, third});
    REQUIRE(history.eventsBetween(at(11), at(20)) == std::vector{third});
    REQUIRE(history.eventsBetween(at(0), at(9)).empty());
    REQUIRE(history.eventsBetween(at(21), at(100)).empty());
    auto selection = history.eventsBetween(at(0), at(20));
    selection.clear();
    REQUIRE(history.events().size() == 3);
    for (const auto interval :
         {std::pair{at(-1), at(10)}, std::pair{at(20), at(10)}, std::pair{at(-2), at(-1)}}) {
        REQUIRE_THROWS_AS(history.eventsBetween(interval.first, interval.second),
                          std::invalid_argument);
    }
}

TEST_CASE("TrackingHistory ignores informational branch identifiers for location",
          "[tracking_history]") {
    TrackingHistory history;
    history.append(TrackingEvent(1, TrackingEventKind::Information, at(0), "B", "Сообщение"));
    REQUIRE_FALSE(history.lastKnownBranch().has_value());
    REQUIRE(history.timeSpentAt("B", at(100)) == 0s);
    history.append(accepted(2, 10, "A"));
    history.append(TrackingEvent(3, TrackingEventKind::Information, at(20), "B", "Сообщение"));
    history.append(TrackingEvent(4, TrackingEventKind::PreparedForDispatch, at(30), "A"));
    REQUIRE(history.lastKnownBranch() == std::optional<std::string>{"A"});
    REQUIRE(history.timeSpentAt("A", at(40)) == 30s);
    REQUIRE(history.timeSpentAt("B", at(40)) == 0s);
    history.append(TrackingEvent(5, TrackingEventKind::Dispatched, at(50), "A"));
    REQUIRE(history.lastKnownBranch() == std::optional<std::string>{"A"});
    REQUIRE(history.timeSpentAt("A", at(100)) == 40s);
    history.append(TrackingEvent(6, TrackingEventKind::Arrived, at(70), "B"));
    REQUIRE(history.lastKnownBranch() == std::optional<std::string>{"B"});
}

TEST_CASE("TrackingHistory measures repeated stays and open intervals", "[tracking_history]") {
    TrackingHistory history;
    history.append(accepted(1, 10));
    history.append(TrackingEvent(2, TrackingEventKind::Dispatched, at(30), "A"));
    history.append(TrackingEvent(3, TrackingEventKind::Arrived, at(50), "B"));
    history.append(
        TrackingEvent(4, TrackingEventKind::DeliveryFailed, at(60), "B", "Нет получателя"));
    history.append(TrackingEvent(5, TrackingEventKind::OutForDelivery, at(70), "B"));
    history.append(TrackingEvent(6, TrackingEventKind::Arrived, at(90), "B"));
    history.append(TrackingEvent(7, TrackingEventKind::AwaitingPickup, at(100), "B"));
    history.append(TrackingEvent(8, TrackingEventKind::ReturnStarted, at(110), "B"));
    history.append(TrackingEvent(9, TrackingEventKind::Dispatched, at(120), "B"));
    history.append(TrackingEvent(10, TrackingEventKind::Returned, at(140), "A"));
    history.append(TrackingEvent(11, TrackingEventKind::Delivered, at(160), "A"));
    REQUIRE(history.timeSpentAt("A", at(0)) == 0s);
    REQUIRE(history.timeSpentAt("A", at(10)) == 0s);
    REQUIRE(history.timeSpentAt("A", at(20)) == 10s);
    REQUIRE(history.timeSpentAt("A", at(30)) == 20s);
    REQUIRE(history.timeSpentAt("A", at(149)) == 29s);
    REQUIRE(history.timeSpentAt(" A ", at(180)) == 40s);
    REQUIRE(history.timeSpentAt("a", at(180)) == 0s);
    REQUIRE(history.timeSpentAt("B", at(49)) == 0s);
    REQUIRE(history.timeSpentAt("B", at(65)) == 15s);
    REQUIRE(history.timeSpentAt("B", at(85)) == 20s);
    REQUIRE(history.timeSpentAt("B", at(95)) == 25s);
    REQUIRE(history.timeSpentAt("B", at(119)) == 49s);
    REQUIRE(history.timeSpentAt("B", at(180)) == 50s);
    REQUIRE(history.lastKnownBranch() == std::optional<std::string>{"A"});
}

TEST_CASE("TrackingHistory stops stays on cancellation or loss without a branch",
          "[tracking_history]") {
    for (const auto kind : {TrackingEventKind::Cancelled, TrackingEventKind::Lost}) {
        TrackingHistory history;
        history.append(accepted(1, 100));
        history.append(TrackingEvent(2, kind, at(200), std::nullopt, "Причина"));
        REQUIRE(history.timeSpentAt("A", at(300)) == 100s);
        REQUIRE(history.lastKnownBranch() == std::optional<std::string>{"A"});
    }
}

TEST_CASE("TrackingHistory matches independent interval formulas at every second",
          "[tracking_history]") {
    TrackingHistory history;
    history.append(accepted(1, 0));
    history.append(TrackingEvent(2, TrackingEventKind::Information, at(3), "B", "Сообщение"));
    history.append(TrackingEvent(3, TrackingEventKind::Dispatched, at(10), "A"));
    history.append(TrackingEvent(4, TrackingEventKind::Arrived, at(20), "B"));
    history.append(TrackingEvent(5, TrackingEventKind::OutForDelivery, at(30), "B"));
    history.append(TrackingEvent(6, TrackingEventKind::Returned, at(35), "A"));
    history.append(TrackingEvent(7, TrackingEventKind::Arrived, at(40), "B"));
    history.append(TrackingEvent(8, TrackingEventKind::Lost, at(45), std::nullopt, "Утрата"));
    // Эталон: A=[0,10)+[35,40), B=[20,30)+[40,45), без повторения алгоритма обхода.
    for (std::int64_t time = 0; time <= 60; ++time) {
        CAPTURE(time);
        const auto expectedA = std::clamp(time, std::int64_t{0}, std::int64_t{10}) +
                               std::clamp(time - 35, std::int64_t{0}, std::int64_t{5});
        const auto expectedB = std::clamp(time - 20, std::int64_t{0}, std::int64_t{10}) +
                               std::clamp(time - 40, std::int64_t{0}, std::int64_t{5});
        REQUIRE(history.timeSpentAt("A", at(time)) == seconds{expectedA});
        REQUIRE(history.timeSpentAt("B", at(time)) == seconds{expectedB});
    }
}

TEST_CASE("TrackingHistory handles extreme timestamps without duration overflow",
          "[tracking_history]") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    TrackingHistory history;
    history.append(accepted(1, 0));
    history.append(TrackingEvent(2, TrackingEventKind::Arrived, at(maximum - 1), "B"));
    history.append(TrackingEvent(3, TrackingEventKind::Delivered, at(maximum), "B"));
    REQUIRE(history.timeSpentAt("A", at(maximum)) == seconds{maximum - 1});
    REQUIRE(history.timeSpentAt("B", at(maximum)) == 1s);
    REQUIRE(history.eventsBetween(at(maximum), at(maximum)).size() == 1);
    TrackingHistory open;
    open.append(accepted(maximum, 0));
    REQUIRE(open.timeSpentAt("A", at(maximum)) == seconds{maximum});
    REQUIRE_THROWS_AS(open.append(accepted(maximum, 1)), post::InvalidShipmentStateException);
}

TEST_CASE("TrackingHistory rejects invalid queries without changing stored events",
          "[tracking_history]") {
    TrackingHistory history;
    history.append(accepted(1, 10));
    const auto snapshot = history;
    for (const auto& branch :
         std::vector<std::string>{"", " \n ", "A B", "\x7f", std::string("A\0B", 3)}) {
        REQUIRE_THROWS_AS(history.timeSpentAt(branch, at(100)), std::invalid_argument);
        REQUIRE(history == snapshot);
    }
    REQUIRE_THROWS_AS(history.timeSpentAt("A", at(-1)), std::invalid_argument);
    REQUIRE_THROWS_AS(history.eventsBetween(at(10), at(0)), std::invalid_argument);
    REQUIRE(history == snapshot);
}

TEST_CASE("TrackingHistory copies and appended events are independent snapshots",
          "[tracking_history]") {
    TrackingHistory original;
    auto event = accepted(1, 10);
    original.append(event);
    event = accepted(2, 20, "B");
    REQUIRE(original.events()[0].branchId() == std::optional<std::string>{"A"});
    auto copy = original;
    REQUIRE(copy == original);
    copy.append(event);
    REQUIRE(copy.events().size() == 2);
    REQUIRE(original.events().size() == 1);
    REQUIRE(original.lastKnownBranch() == std::optional<std::string>{"A"});
    REQUIRE(copy.lastKnownBranch() == std::optional<std::string>{"B"});
    REQUIRE(original.timeSpentAt("A", at(30)) == 20s);
    REQUIRE(copy.timeSpentAt("A", at(30)) == 10s);
    auto assigned = TrackingHistory{};
    assigned = copy;
    assigned.append(TrackingEvent(3, TrackingEventKind::Dispatched, at(30), "B"));
    REQUIRE(assigned.events().size() == 3);
    REQUIRE(copy.events().size() == 2);
}
