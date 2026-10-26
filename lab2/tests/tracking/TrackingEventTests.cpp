#include "post/tracking/TrackingEvent.hpp"

#include "post/exceptions/InvalidShipmentDataException.hpp"
#include "post/exceptions/PostalException.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

using post::TrackingEvent;
using post::TrackingEventKind;
using namespace std::chrono;

namespace {
constexpr std::array kinds{
    TrackingEventKind::Accepted,       TrackingEventKind::PreparedForDispatch,
    TrackingEventKind::Dispatched,     TrackingEventKind::Arrived,
    TrackingEventKind::OutForDelivery, TrackingEventKind::AwaitingPickup,
    TrackingEventKind::DeliveryFailed, TrackingEventKind::Delivered,
    TrackingEventKind::ReturnStarted,  TrackingEventKind::Returned,
    TrackingEventKind::Cancelled,      TrackingEventKind::Lost,
    TrackingEventKind::Information};
} // namespace

TEST_CASE("TrackingEvent normalizes text and exposes read-only data", "[tracking_event]") {
    const TrackingEvent event(5, TrackingEventKind::Accepted, sys_seconds{seconds{123}}, " BR-1 ",
                              " Принято   в отделении ");
    REQUIRE(event.sequence() == 5);
    REQUIRE(event.kind() == TrackingEventKind::Accepted);
    REQUIRE(event.occurredAt() == sys_seconds{seconds{123}});
    REQUIRE(event.branchId() == std::optional<std::string>{"BR-1"});
    REQUIRE(event.description() == "Принято в отделении");
    REQUIRE_NOTHROW(event.validate());
    STATIC_REQUIRE(std::is_same_v<decltype(event.branchId()), const std::optional<std::string>&>);
    STATIC_REQUIRE(std::is_same_v<decltype(event.description()), const std::string&>);
}

TEST_CASE("TrackingEvent supports every declared kind", "[tracking_event]") {
    for (const auto kind : kinds) {
        const TrackingEvent event(1, kind, sys_seconds{}, "BR-1", "Описание");
        REQUIRE(event.kind() == kind);
        REQUIRE_NOTHROW(event.validate());
    }
    const TrackingEvent accepted(1, TrackingEventKind::Accepted, sys_seconds{}, "BR-1");
    REQUIRE(accepted.description().empty());
}

TEST_CASE("TrackingEvent requires a branch for physical processing", "[tracking_event]") {
    for (const auto kind : kinds) {
        CAPTURE(static_cast<int>(kind));
        const bool optionalBranch = kind == TrackingEventKind::Information ||
                                    kind == TrackingEventKind::Cancelled ||
                                    kind == TrackingEventKind::Lost;
        if (optionalBranch) {
            const TrackingEvent event(1, kind, sys_seconds{}, std::nullopt, "Объяснение");
            REQUIRE_FALSE(event.branchId().has_value());
        } else {
            REQUIRE_THROWS_AS(TrackingEvent(1, kind, sys_seconds{}, std::nullopt, "Описание"),
                              post::InvalidShipmentDataException);
        }
    }
    for (const auto& branch :
         std::vector<std::string>{"", " \t ", "BR 1", "BR\n1", "\x01", std::string("A\0B", 3)}) {
        REQUIRE_THROWS_AS(
            TrackingEvent(1, TrackingEventKind::Information, sys_seconds{}, branch, "Описание"),
            post::InvalidShipmentDataException);
    }
}

TEST_CASE("TrackingEvent requires explanations for failures and messages", "[tracking_event]") {
    for (const auto kind : {TrackingEventKind::DeliveryFailed, TrackingEventKind::Information,
                            TrackingEventKind::Cancelled, TrackingEventKind::Lost}) {
        for (const auto& description : {"", " \n "}) {
            REQUIRE_THROWS_AS(TrackingEvent(1, kind, sys_seconds{}, "BR-1", description),
                              post::InvalidShipmentDataException);
        }
    }
    for (const auto& description : std::vector<std::string>{"\x7f", std::string("A\0B", 3)}) {
        REQUIRE_THROWS_AS(
            TrackingEvent(1, TrackingEventKind::Accepted, sys_seconds{}, "BR-1", description),
            post::InvalidShipmentDataException);
    }
}

TEST_CASE("TrackingEvent validates kinds sequence and timestamp boundaries", "[tracking_event]") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const TrackingEvent boundary(maximum, TrackingEventKind::Accepted,
                                 sys_seconds{seconds{maximum}}, "BR-1");
    REQUIRE(boundary.sequence() == maximum);
    REQUIRE(boundary.occurredAt() == sys_seconds{seconds{maximum}});
    for (const std::int64_t sequence :
         {std::int64_t{0}, std::int64_t{-1}, std::numeric_limits<std::int64_t>::min()}) {
        REQUIRE_THROWS_AS(
            TrackingEvent(sequence, TrackingEventKind::Accepted, sys_seconds{}, "BR-1"),
            post::InvalidShipmentDataException);
    }
    for (const int kind : {-1, 13, 99}) {
        REQUIRE_THROWS_AS(
            TrackingEvent(1, static_cast<TrackingEventKind>(kind), sys_seconds{}, "BR-1"),
            post::InvalidShipmentDataException);
    }
    for (const auto time : {sys_seconds{seconds{-1}},
                            sys_seconds{seconds{std::numeric_limits<std::int64_t>::min()}}}) {
        REQUIRE_THROWS_AS(TrackingEvent(1, TrackingEventKind::Accepted, time, "BR-1"),
                          post::InvalidShipmentDataException);
    }
    REQUIRE_THROWS_AS(TrackingEvent(0, TrackingEventKind::Accepted, sys_seconds{}, "BR-1"),
                      post::PostalException);
}

TEST_CASE("TrackingEvent copies own strings and failed replacement preserves the original",
          "[tracking_event]") {
    std::optional<std::string> branch{"BR-1"};
    std::string description = "Принято";
    const TrackingEvent original(1, TrackingEventKind::Accepted, sys_seconds{}, branch,
                                 description);
    branch = "BR-2";
    description = "Другое описание";
    REQUIRE(original.branchId() == std::optional<std::string>{"BR-1"});
    REQUIRE(original.description() == "Принято");
    auto copy = original;
    REQUIRE(copy == original);
    REQUIRE_THROWS_AS(copy = TrackingEvent(0, TrackingEventKind::Accepted, sys_seconds{}, "BR-1"),
                      post::InvalidShipmentDataException);
    REQUIRE(copy == original);
    copy = TrackingEvent(2, TrackingEventKind::Arrived, sys_seconds{seconds{1}}, "BR-2");
    REQUIRE(copy != original);
    REQUIRE(original.sequence() == 1);
}
