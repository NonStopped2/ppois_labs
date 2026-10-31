#include "../TransportFixtures.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace post;
TEST_CASE("SearchRequest finds a stored shipment with checked metadata and independent copies",
          "[search-request]") {
    ShipmentRegistry registry("BY");
    auto branch = test::branch();
    const auto id = test::registerCargo(registry);
    test::accept(branch, registry, id);
    SearchRequest request(" search ");
    REQUIRE(request.id() == "search");
    REQUIRE(request.status() == SearchStatus::Draft);
    REQUIRE_FALSE(request.openedAt());
    REQUIRE_FALSE(request.closedAt());
    request.open(registry.at(id), registry.at(id).sender(), test::route(), test::moment(20));
    REQUIRE(request.shipmentId() == id);
    REQUIRE(request.applicantCustomerId() == "sender");
    REQUIRE(request.lastKnownBranchId() == "A");
    REQUIRE(request.openedAt() == test::moment(20));
    REQUIRE(request.openedSequence() == 1);
    REQUIRE(request.expectedBranchIds() == std::vector<std::string>{"A", "B"});
    request.recordBranchCheck(branch, "worker", true, " Найдено в ячейке ", test::moment(21));
    REQUIRE(request.checkedBranchIds().contains("A"));
    REQUIRE(std::get<2>(request.findings().at("A")) == "Найдено в ячейке");
    auto copy = request;
    copy.resolveFound(registry.at(id), branch, test::moment(22));
    REQUIRE(copy.status() == SearchStatus::Found);
    REQUIRE(copy.closedAt() == test::moment(22));
    REQUIRE(request.status() == SearchStatus::Open);
    REQUIRE(copy != request);
    REQUIRE_THROWS_AS(
        copy.recordBranchCheck(branch, "worker", false, "Проверено", test::moment(23)),
        InvalidShipmentStateException);
}
TEST_CASE("SearchRequest confirms loss only after all physical checks and releases stale inventory",
          "[search-request]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id);
    SearchRequest request("s");
    request.open(registry.at(id), registry.at(id).recipient(), test::route(), test::moment(20));
    request.recordBranchCheck(source, "worker", false,
                              "Физически отсутствует, учётная запись осталась", test::moment(21));
    const auto partial = request;
    REQUIRE_THROWS_AS(
        request.closeAsLost(source, registry, "worker", std::chrono::seconds{0}, test::moment(22)),
        InvalidShipmentStateException);
    REQUIRE(request == partial);
    request.recordBranchCheck(target, "worker", false, "Не поступало", test::moment(22));
    REQUIRE_THROWS_AS(
        request.closeAsLost(source, registry, "worker", std::chrono::seconds{10}, test::moment(29)),
        InvalidShipmentStateException);
    REQUIRE_THROWS_AS(
        request.closeAsLost(source, registry, "worker", std::chrono::seconds{-1}, test::moment(30)),
        std::invalid_argument);
    request.closeAsLost(source, registry, "worker", std::chrono::seconds{10}, test::moment(30));
    REQUIRE(request.status() == SearchStatus::Lost);
    REQUIRE(request.closedAt() == test::moment(30));
    REQUIRE(registry.at(id).status() == ShipmentStatus::Lost);
    REQUIRE_THROWS_AS(source.storageEntry(id), ShipmentNotFoundException);
    REQUIRE(registry.at(id).history().timeSpentAt("A", test::moment(50)) ==
            std::chrono::seconds{15});
}
TEST_CASE("SearchRequest cannot declare loss after a positive physical finding",
          "[search-request]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id);
    SearchRequest request("s");
    request.open(registry.at(id), registry.at(id).sender(), test::route(), test::moment(20));
    request.recordBranchCheck(source, "worker", true, "Найдено", test::moment(21));
    request.recordBranchCheck(target, "worker", false, "Нет", test::moment(22));
    const auto before = request;
    REQUIRE_THROWS_AS(
        request.closeAsLost(source, registry, "worker", std::chrono::seconds{0}, test::moment(23)),
        InvalidShipmentStateException);
    REQUIRE(request == before);
    REQUIRE(registry.at(id).status() == ShipmentStatus::Accepted);
}
TEST_CASE("SearchRequest rejects unrelated terminal stale duplicate and premature evidence",
          "[search-request]") {
    ShipmentRegistry registry("BY");
    auto branch = test::branch();
    auto target = test::branch("B", "23");
    const auto id = test::registerCargo(registry);
    test::accept(branch, registry, id);
    SearchRequest request("s");
    REQUIRE_THROWS_AS(
        request.open(registry.at(id), test::customer("stranger"), test::route(), test::moment(20)),
        DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(request.open(registry.at(id), registry.at(id).sender(),
                                   test::routeBetween("B", "C"), test::moment(20)),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(
        request.open(registry.at(id), registry.at(id).sender(), test::route(), test::moment(14)),
        InvalidShipmentStateException);
    for (const auto state :
         {ShipmentStatus::Draft, ShipmentStatus::Delivered, ShipmentStatus::Returned,
          ShipmentStatus::Cancelled, ShipmentStatus::Lost}) {
        const auto other = test::atStatus(state);
        SearchRequest invalid("x");
        REQUIRE_THROWS_AS(invalid.open(other.at("BY-1"), other.at("BY-1").sender(), test::route(),
                                       test::moment(100)),
                          InvalidShipmentStateException);
    }
    request.open(registry.at(id), registry.at(id).sender(), test::route(), test::moment(20));
    REQUIRE_THROWS_AS(request.recordBranchCheck(branch, "worker", false, " ", test::moment(21)),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        request.recordBranchCheck(target, "worker", true, "Найдено", test::moment(21)),
        ShipmentNotFoundException);
    REQUIRE_THROWS_AS(request.recordBranchCheck(test::branch("C", "24"), "worker", false, "Нет",
                                                test::moment(21)),
                      InvalidShipmentStateException);
    request.recordBranchCheck(branch, "worker", false, "Нет", test::moment(21));
    REQUIRE_THROWS_AS(request.recordBranchCheck(branch, "worker", false, "Нет", test::moment(22)),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(request.recordBranchCheck(target, "worker", false, "Нет", test::moment(20)),
                      InvalidShipmentStateException);
    request.recordBranchCheck(target, "worker", false, "Нет", test::moment(22));
    const auto before = request;
    const auto warehouse = branch;
    detail::appendFact(registry.at(id), TrackingEventKind::Information, test::moment(22), "A",
                       "Новое событие с той же датой");
    REQUIRE_THROWS_AS(
        request.closeAsLost(branch, registry, "worker", std::chrono::seconds{0}, test::moment(23)),
        InvalidShipmentStateException);
    REQUIRE(request == before);
    REQUIRE(branch == warehouse);
    REQUIRE(registry.at(id).status() == ShipmentStatus::Accepted);
}
