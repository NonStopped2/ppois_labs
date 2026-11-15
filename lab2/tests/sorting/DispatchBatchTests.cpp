#include "../BranchFixtures.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/TransportUnavailableException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("DispatchBatch validates composition route and lifecycle", "[dispatch-batch]") {
    ShipmentRegistry registry("BY");
    const auto id = test::registerCargo(registry);
    auto source = test::branch();
    auto target = test::branch("B", "23");
    test::accept(source, registry, id);
    MailBag bag("bag", 100, "B");
    DispatchBatch batch(" batch ", " A ", " B ", test::moment(20));
    REQUIRE(batch.id() == "batch");
    REQUIRE(batch.sourceBranchId() == "A");
    REQUIRE(batch.targetBranchId() == "B");
    REQUIRE(batch.createdAt() == test::moment(20));
    REQUIRE(batch.status() == BatchStatus::Draft);
    REQUIRE(batch.bags().empty());
    REQUIRE_THROWS_AS(batch.addBag(bag), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(batch.prepareForDispatch(test::route()), InvalidShipmentStateException);
    bag.add(registry.at(id), target, test::sortingPlan());
    bag.seal("s", test::moment(20), registry);
    batch.addBag(bag);
    REQUIRE_THROWS_AS(batch.addBag(bag), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(batch.prepareForDispatch(test::routeBetween("B", "A")),
                      TransportUnavailableException);
    batch.prepareForDispatch(test::route());
    REQUIRE(batch.status() == BatchStatus::Ready);
    REQUIRE_THROWS_AS(batch.addBag(bag), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(batch.prepareForDispatch(test::route()), InvalidShipmentStateException);
    REQUIRE_THROWS_AS(batch.markReceived(target, registry, test::moment(25)),
                      InvalidShipmentStateException);
    source.transferOut(registry, batch, test::sortingPlan(), "worker", test::moment(30));
    REQUIRE(batch.status() == BatchStatus::InTransit);
    REQUIRE_THROWS_AS(batch.markReceived(target, registry, test::moment(40)),
                      InvalidShipmentStateException);
    target.receiveBatch(registry, batch, "worker", test::storagePolicy(), test::moment(2200));
    REQUIRE(batch.status() == BatchStatus::Received);
    REQUIRE_THROWS_AS(batch.markReceived(target, registry, test::moment(2200)),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(DispatchBatch("b", "A", "A", test::moment(0)), std::invalid_argument);
}
TEST_CASE(
    "DispatchBatch rejects duplicate cargo across differently named bags and other destinations",
    "[dispatch-batch]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto target = test::branch("B", "23");
    MailBag first("first", 100, "B"), second("second", 100, "B");
    for (auto* bag : {&first, &second}) {
        bag->add(registry.at("BY-1"), target, test::sortingPlan());
        bag->seal("seal-" + bag->id(), test::moment(5), registry);
    }
    DispatchBatch batch("b", "A", "B", test::moment(0));
    batch.addBag(first);
    const auto before = batch;
    REQUIRE_THROWS_AS(batch.addBag(second), InvalidShipmentStateException);
    REQUIRE(batch == before);
    REQUIRE_THROWS_AS(DispatchBatch("c", "A", "C", test::moment(0)).addBag(first),
                      InvalidShipmentStateException);
    auto copy = batch;
    copy.prepareForDispatch(test::route());
    REQUIRE(copy != batch);
    REQUIRE(batch.status() == BatchStatus::Draft);
}
