#include "../TransportFixtures.hpp"
#include "post/exceptions/ManifestMismatchException.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace post;
TEST_CASE("TransportManifest detects changed mass with matching batch and seal identifiers",
          "[transport-manifest]") {
    auto original = test::atStatus(ShipmentStatus::Accepted);
    const auto target = test::branch("B", "23");
    const auto car = test::vehicle();
    TransportTrip trip("t", test::route());
    trip.loadBatch(test::batchFor(original, target, {"BY-1"}), car);
    const auto manifest = TransportManifest::buildFor("m", trip, test::moment(20));
    ShipmentRegistry changed("BY");
    const auto id = test::registerCargo(changed, 21);
    test::advance(changed.at(id), TrackingEventKind::Accepted);
    TransportTrip other("t", test::route());
    other.loadBatch(test::batchFor(changed, target, {id}), car);
    REQUIRE_THROWS_AS(manifest.verifyAgainst(other), ManifestMismatchException);
    MailBag bag("bag-batch", 100, "B");
    bag.add(original.at("BY-1"), target, test::sortingPlan());
    bag.seal("different", test::moment(20), original);
    DispatchBatch batch("batch", "A", "B", test::moment(20));
    batch.addBag(bag);
    batch.prepareForDispatch(test::route());
    TransportTrip changedSeal("t", test::route());
    changedSeal.loadBatch(batch, car);
    REQUIRE_THROWS_AS(manifest.verifyAgainst(changedSeal), ManifestMismatchException);
}
TEST_CASE("TransportManifest owns independent batch seal and mass snapshots",
          "[transport-manifest]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto target = test::branch("B", "23");
    const auto car = test::vehicle();
    TransportTrip trip("t", test::route());
    REQUIRE_THROWS_AS(TransportManifest::buildFor("m", trip, test::moment(20)),
                      ManifestMismatchException);
    trip.loadBatch(test::batchFor(registry, target, {"BY-1"}), car);
    REQUIRE_THROWS_AS(TransportManifest::buildFor("m", trip, test::moment(19)),
                      ManifestMismatchException);
    const auto manifest = TransportManifest::buildFor(" manifest ", trip, test::moment(20));
    REQUIRE(manifest.id() == "manifest");
    REQUIRE(manifest.tripId() == "t");
    REQUIRE(manifest.issuedAt() == test::moment(20));
    REQUIRE(manifest.batchIds() == std::vector<std::string>{"batch"});
    REQUIRE(manifest.seals().at("bag-batch") == "seal-batch");
    REQUIRE(manifest.declaredWeightGrams() == 20);
    REQUIRE_NOTHROW(manifest.verifyAgainst(trip));
    REQUIRE(manifest == manifest);
    TransportTrip foreign("foreign", test::route());
    foreign.loadBatch(trip.batches().front(), car);
    REQUIRE_THROWS_AS(manifest.verifyAgainst(foreign), ManifestMismatchException);
    TransportTrip missing("t", test::route());
    REQUIRE_THROWS_AS(manifest.verifyAgainst(missing), ManifestMismatchException);
    TransportTrip changed("t", test::route());
    changed.loadBatch(test::batchFor(registry, target, {"BY-1"}, "replacement"), car);
    REQUIRE_THROWS_AS(manifest.verifyAgainst(changed), ManifestMismatchException);
    REQUIRE(manifest != TransportManifest::buildFor("another", trip, test::moment(20)));
}
