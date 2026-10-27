#include "../LogisticsFixtures.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"
#include "post/exceptions/StorageCapacityException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("StorageCell checks exact count weight and dimensional limits", "[storage-cell]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    StorageCell cell(" cell ", 20, 1, Dimensions(5, 100, 200));
    REQUIRE(cell.id() == "cell");
    REQUIRE(cell.maxWeightGrams() == 20);
    REQUIRE(cell.maxItems() == 1);
    REQUIRE(cell.limit() == Dimensions(200, 100, 5));
    cell.place(registry.at("BY-1"), test::storagePolicy(), test::moment(10));
    REQUIRE(cell.totalWeightGrams() == 20);
    REQUIRE(cell.entries().size() == 1);
    REQUIRE(cell.contains("BY-1"));
    const auto before = cell;
    REQUIRE_THROWS_AS(cell.place(registry.at("BY-1"), test::storagePolicy(), test::moment(10)),
                      StorageCapacityException);
    REQUIRE(cell == before);
    const auto entry = cell.take(" BY-1 ");
    REQUIRE(entry.shipmentId() == "BY-1");
    REQUIRE(cell.totalWeightGrams() == 0);
    REQUIRE(cell.entries().empty());
    REQUIRE(cell != before);
    REQUIRE_THROWS_AS(cell.take("BY-1"), ShipmentNotFoundException);
}
TEST_CASE("StorageCell rejects each capacity violation and retains independent copies",
          "[storage-cell]") {
    auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto other = test::registerLetter(registry);
    test::advance(registry.at(other), TrackingEventKind::Accepted);
    const auto policy = test::storagePolicy();
    for (auto cell : {StorageCell("weight", 19, 10, Dimensions(200, 100, 5)),
                      StorageCell("size", 100, 10, Dimensions(200, 100, 4))}) {
        const auto before = cell;
        REQUIRE_THROWS_AS(cell.place(registry.at("BY-1"), policy, test::moment(10)),
                          StorageCapacityException);
        REQUIRE(cell == before);
    }
    StorageCell count("count", 100, 1, Dimensions(200, 100, 5));
    count.place(registry.at("BY-1"), policy, test::moment(10));
    REQUIRE_THROWS_AS(count.place(registry.at(other), policy, test::moment(10)),
                      StorageCapacityException);
    auto copy = count;
    (void)copy.take("BY-1");
    REQUIRE(count.entries().size() == 1);
    REQUIRE_THROWS_AS(StorageCell("x", 0, 1, Dimensions(1, 1, 1)), std::invalid_argument);
    REQUIRE_THROWS_AS(StorageCell("x", 1, 0, Dimensions(1, 1, 1)), std::invalid_argument);
}
