#include "../Fixtures.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/IncompatibleServiceException.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <set>
#include <type_traits>
#include <utility>

using namespace post;
using namespace std::chrono;
namespace {
using Status = ShipmentStatus;
using Event = TrackingEventKind;

// Независимый перечень разрешённых пар проверяет все 12 * 13 сочетаний автомата.
bool allowed(Status status, Event event) {
    if (event == Event::Information) {
        return true;
    }
    switch (status) {
    case Status::Draft:
        return event == Event::Accepted || event == Event::Cancelled;
    case Status::Accepted:
        return event == Event::PreparedForDispatch || event == Event::ReturnStarted ||
               event == Event::Lost;
    case Status::PreparedForDispatch:
        return event == Event::Dispatched || event == Event::ReturnStarted || event == Event::Lost;
    case Status::InTransit:
        return event == Event::Arrived || event == Event::Lost;
    case Status::Arrived:
        return std::set{Event::PreparedForDispatch, Event::OutForDelivery, Event::AwaitingPickup,
                        Event::ReturnStarted, Event::Lost}
            .contains(event);
    case Status::OutForDelivery:
        return std::set{Event::Delivered, Event::DeliveryFailed, Event::Arrived, Event::Lost}
            .contains(event);
    case Status::AwaitingPickup:
        return std::set{Event::OutForDelivery, Event::Delivered, Event::ReturnStarted, Event::Lost}
            .contains(event);
    case Status::Returning:
        return std::set{Event::PreparedForDispatch, Event::Dispatched, Event::Arrived,
                        Event::Returned, Event::Lost}
            .contains(event);
    default:
        return false;
    }
}
Status expected(Status current, Event event) {
    if (event == Event::Information || event == Event::DeliveryFailed) {
        return current;
    }
    if (current == Status::Returning && event != Event::Lost && event != Event::Returned) {
        return current;
    }
    switch (event) {
    case Event::Accepted:
        return Status::Accepted;
    case Event::PreparedForDispatch:
        return Status::PreparedForDispatch;
    case Event::Dispatched:
        return Status::InTransit;
    case Event::Arrived:
        return Status::Arrived;
    case Event::OutForDelivery:
        return Status::OutForDelivery;
    case Event::AwaitingPickup:
        return Status::AwaitingPickup;
    case Event::Delivered:
        return Status::Delivered;
    case Event::ReturnStarted:
        return Status::Returning;
    case Event::Returned:
        return Status::Returned;
    case Event::Cancelled:
        return Status::Cancelled;
    default:
        return Status::Lost;
    }
}
void checkTransition(Status status, Event event) {
    auto registry = test::atStatus(status);
    auto& shipment = registry.at("BY-1");
    if (status == Status::Returning && (event == Event::Dispatched || event == Event::Arrived)) {
        test::advance(shipment, Event::PreparedForDispatch);
        if (event == Event::Arrived) {
            test::advance(shipment, Event::Dispatched);
        }
    }
    const auto before = shipment.history();
    const auto branch = shipment.history().lastKnownBranch().value_or("A");
    CAPTURE(status, event);
    if (allowed(status, event)) {
        REQUIRE_NOTHROW(test::advance(shipment, event, branch));
        REQUIRE(shipment.status() == expected(status, event));
        REQUIRE(shipment.history().events().size() == before.events().size() + 1);
    } else {
        REQUIRE_THROWS_AS(test::advance(shipment, event, branch), InvalidShipmentStateException);
        REQUIRE(shipment.status() == status);
        REQUIRE(shipment.history() == before);
    }
}
} // namespace

TEST_CASE("Shipment identity cannot be replaced through base or product assignment", "[shipment]") {
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<Shipment>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<Letter>);
    STATIC_REQUIRE_FALSE(std::is_copy_assignable_v<Parcel>);
    STATIC_REQUIRE(std::is_copy_constructible_v<Letter>);
    STATIC_REQUIRE(std::is_copy_constructible_v<Parcel>);
    ShipmentRegistry registry("BY");
    const auto id = registry.registerShipment(test::parcel(), 1000, Dimensions(500, 500, 500), 10);
    auto& original = dynamic_cast<Parcel&>(registry.at(id));
    const auto contents = original.contents();
    Parcel snapshot(std::move(original));
    REQUIRE(original.contents() == contents);
    REQUIRE(original.sealId() == "seal-1");
    REQUIRE(original.trackingNumber() == id);
    snapshot.revise(600, DeliveryOptions{});
    REQUIRE(original.weightGrams() == 500);
    REQUIRE(snapshot.weightGrams() == 600);
}
TEST_CASE("Shipment checks every state/event pair atomically", "[shipment]") {
    for (int status = 0; status <= static_cast<int>(Status::Lost); ++status) {
        for (int event = 0; event <= static_cast<int>(Event::Information); ++event) {
            checkTransition(static_cast<Status>(status), static_cast<Event>(event));
        }
    }
}
TEST_CASE("Shipment owns customer snapshots and checks revision changes", "[shipment]") {
    auto source = test::letter();
    REQUIRE(source.sender() == test::customer("sender"));
    REQUIRE(source.recipient() == test::customer("recipient"));
    REQUIRE(source.trackingNumber().empty());
    REQUIRE_FALSE(source.acceptedAt().has_value());
    source.revise(30, DeliveryOptions(true, Money(100), true));
    REQUIRE(source.weightGrams() == 30);
    REQUIRE(source.options().registered());
    REQUIRE(source.revision() == 1);
    REQUIRE_THROWS_AS(source.revise(0, DeliveryOptions{}), InvalidShipmentDataException);
    REQUIRE_THROWS_AS(source.revise(50, DeliveryOptions(false, Money{}, false, true)),
                      IncompatibleServiceException);
    REQUIRE(source.weightGrams() == 30);
    REQUIRE(source.revision() == 1);
}
TEST_CASE("Shipment rejects acceptance before registration and wrong branches", "[shipment]") {
    auto source = test::letter();
    REQUIRE_THROWS_AS(test::advance(source, Event::Accepted), InvalidShipmentStateException);
    REQUIRE(source.history().events().empty());
    auto registry = test::atStatus(Status::Accepted);
    auto& shipment = registry.at("BY-1");
    const auto history = shipment.history();
    REQUIRE(shipment.acceptedAt() == sys_seconds{seconds{1}});
    REQUIRE_THROWS_AS(test::advance(shipment, Event::PreparedForDispatch, "wrong"),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(shipment.revise(30, DeliveryOptions{}), InvalidShipmentStateException);
    REQUIRE(shipment.history() == history);
}
TEST_CASE("Shipment preserves state when chronology fails", "[shipment]") {
    auto registry = test::atStatus(Status::Accepted);
    auto& shipment = registry.at("BY-1");
    const auto history = shipment.history();
    REQUIRE_THROWS_AS(shipment.applyTrackingEvent(TrackingEvent(1, Event::PreparedForDispatch,
                                                                sys_seconds{seconds{1}}, "A")),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(shipment.applyTrackingEvent(
                          TrackingEvent(2, Event::PreparedForDispatch, sys_seconds{}, "A")),
                      InvalidShipmentStateException);
    REQUIRE(shipment.status() == Status::Accepted);
    REQUIRE(shipment.history() == history);
    REQUIRE(shipment.acceptedAt() == sys_seconds{seconds{1}});
}
TEST_CASE("Shipment cancellation rejects repeats, empty reasons and sequence overflow",
          "[shipment]") {
    auto shipment = test::letter();
    REQUIRE_THROWS(shipment.cancelBeforeAcceptance(sys_seconds{}, " "));
    REQUIRE(shipment.status() == Status::Draft);
    shipment.cancelBeforeAcceptance(sys_seconds{}, "Не нужно");
    REQUIRE(shipment.status() == Status::Cancelled);
    REQUIRE_THROWS_AS(shipment.cancelBeforeAcceptance(sys_seconds{}, "Повтор"),
                      InvalidShipmentStateException);
    auto exhausted = test::letter();
    exhausted.applyTrackingEvent(TrackingEvent(std::numeric_limits<std::int64_t>::max(),
                                               Event::Information, sys_seconds{}, std::nullopt,
                                               "Примечание"));
    REQUIRE_THROWS_AS(exhausted.cancelBeforeAcceptance(sys_seconds{}, "Отмена"),
                      std::overflow_error);
    REQUIRE(exhausted.status() == Status::Draft);
}
TEST_CASE("Shipment return must end at its original branch", "[shipment]") {
    auto registry = test::atStatus(Status::Arrived);
    auto& shipment = registry.at("BY-1");
    test::advance(shipment, Event::ReturnStarted, "B");
    const auto before = shipment.history();
    REQUIRE_THROWS_AS(test::advance(shipment, Event::Returned, "B"), InvalidShipmentStateException);
    REQUIRE(shipment.history() == before);
    test::advance(shipment, Event::PreparedForDispatch, "B");
    test::advance(shipment, Event::Dispatched, "B");
    test::advance(shipment, Event::Arrived, "A");
    test::advance(shipment, Event::Returned, "A");
    REQUIRE(shipment.status() == Status::Returned);
    REQUIRE(shipment.acceptedAt() == sys_seconds{seconds{1}});
}
TEST_CASE("Returning shipments cannot skip or repeat transport phases", "[shipment]") {
    auto registry = test::atStatus(Status::Returning);
    auto& shipment = registry.at("BY-1");
    REQUIRE_THROWS_AS(test::advance(shipment, Event::Dispatched), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(test::advance(shipment, Event::Arrived), InvalidShipmentStateException);
    test::advance(shipment, Event::PreparedForDispatch);
    REQUIRE_THROWS_AS(test::advance(shipment, Event::Returned), InvalidShipmentStateException);
    test::advance(shipment, Event::Dispatched);
    REQUIRE_THROWS_AS(test::advance(shipment, Event::Dispatched), InvalidShipmentStateException);
    test::advance(shipment, Event::Information);
    test::advance(shipment, Event::Arrived);
    test::advance(shipment, Event::Returned);
    REQUIRE(shipment.status() == Status::Returned);
}
TEST_CASE("Shared identifier and time checks handle boundaries", "[shipment]") {
    REQUIRE(detail::identifier(" A ") == "A");
    for (const auto& invalid : {"", "a b", "\x01"}) {
        REQUIRE_THROWS_AS(detail::identifier(invalid), std::invalid_argument);
    }
    REQUIRE(detail::after(sys_seconds{}, seconds{1}) == sys_seconds{seconds{1}});
    REQUIRE_THROWS_AS(detail::after(sys_seconds{seconds{-1}}, seconds{1}), std::invalid_argument);
    REQUIRE_THROWS_AS(detail::after(sys_seconds{}, seconds{-1}), std::overflow_error);
    REQUIRE_THROWS_AS(detail::after(sys_seconds{seconds::max()}, seconds{1}), std::overflow_error);
}
