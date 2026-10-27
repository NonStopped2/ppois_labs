#include "../DeliveryFixtures.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/PaymentException.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace post;
TEST_CASE("CompensationClaim decisions require live authority rather than a manager card alone",
          "[compensation-claim]") {
    const auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto& shipment = registry.at("BY-1");
    auto branch = test::branch();
    CompensationClaim claim("c");
    claim.submit(shipment, shipment.sender(), Money(10), CompensationReason::Damaged, {"photo"},
                 test::moment(10));
    branch.addShiftBreak("shift", test::moment(20), test::moment(30));
    const auto before = claim;
    REQUIRE_THROWS_AS(
        claim.approve(shipment, branch, "worker", Money(10), Money(10), test::moment(20)),
        DeliveryAuthorizationException);
    REQUIRE(claim == before);
    REQUIRE_THROWS_AS(
        claim.reject(branch, "worker", "Недостаточно доказательств", test::moment(20)),
        DeliveryAuthorizationException);
    REQUIRE(claim == before);
    claim.approve(shipment, branch, "worker", Money(10), Money(10), test::moment(30));
    REQUIRE(claim.status() == CompensationStatus::Approved);
}
TEST_CASE("CompensationClaim requires closed loss search and pays only the approved amount once",
          "[compensation-claim]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id);
    const auto search = test::confirmLost(registry, source, id);
    CompensationClaim claim(" claim ");
    REQUIRE(claim.id() == "claim");
    REQUIRE(claim.status() == CompensationStatus::Draft);
    REQUIRE_FALSE(claim.submittedAt());
    REQUIRE_FALSE(claim.decidedAt());
    REQUIRE_FALSE(claim.paidAt());
    claim.submit(registry.at(id), registry.at(id).sender(), Money(100), CompensationReason::Lost,
                 {"proof-1"}, test::moment(24));
    REQUIRE(claim.shipmentId() == id);
    REQUIRE(claim.claimantCustomerId() == "sender");
    REQUIRE(claim.claimedAmount() == Money(100));
    REQUIRE(claim.approvedAmount() == Money{});
    REQUIRE(claim.reason() == CompensationReason::Lost);
    REQUIRE(claim.evidenceReferences() == std::vector<std::string>{"proof-1"});
    REQUIRE(claim.submittedAt() == test::moment(24));
    REQUIRE_THROWS_AS(claim.assess(registry.at(id), Money(100)), PaymentException);
    REQUIRE(claim.assess(registry.at(id), Money(80), &search) == Money(80));
    const auto before = claim;
    REQUIRE_THROWS_AS(claim.approve(registry.at(id), source, "worker", Money(81), Money(80),
                                    test::moment(25), &search),
                      PaymentException);
    REQUIRE(claim == before);
    claim.approve(registry.at(id), source, "worker", Money(80), Money(80), test::moment(25),
                  &search);
    REQUIRE_FALSE(claim.decisionReason().empty());
    REQUIRE(claim.decidedAt() == test::moment(25));
    REQUIRE_THROWS_AS(claim.markPaid(Money(79), test::moment(26)), PaymentException);
    REQUIRE_THROWS_AS(claim.markPaid(Money(80), test::moment(24)), PaymentException);
    claim.markPaid(Money(80), test::moment(26));
    REQUIRE(claim.status() == CompensationStatus::Paid);
    REQUIRE(claim.paidAt() == test::moment(26));
    REQUIRE_THROWS_AS(claim.markPaid(Money(80), test::moment(27)), PaymentException);
}
TEST_CASE("CompensationClaim caps declared value and rejects unprivileged or repeated decisions",
          "[compensation-claim]") {
    ShipmentRegistry registry("BY");
    const Letter prototype(test::customer("sender"), test::customer("recipient"), 20,
                           DeliveryOptions(true, Money(500), true), Dimensions(200, 100, 5), 1,
                           "Документы");
    const auto id = registry.registerShipment(prototype, 100, Dimensions(200, 100, 5), 10);
    test::advance(registry.at(id), TrackingEventKind::Accepted);
    CompensationClaim claim("c");
    claim.submit(registry.at(id), registry.at(id).recipient(), Money(700),
                 CompensationReason::Damaged, {"photo"}, test::moment(10));
    REQUIRE(claim.assess(registry.at(id), Money(800)) == Money(500));
    REQUIRE_THROWS_AS(claim.approve(registry.at(id),
                                    test::branch("A", "22", 10, EmployeeRole::Clerk), "worker",
                                    Money(500), Money(800), test::moment(20)),
                      DeliveryAuthorizationException);
    auto rejected = claim;
    REQUIRE_THROWS_AS(rejected.reject(test::branch(), "worker", " ", test::moment(20)),
                      PaymentException);
    rejected.reject(test::branch(), "worker", " Доказательства не подтверждены ", test::moment(20));
    REQUIRE(rejected.status() == CompensationStatus::Rejected);
    REQUIRE(rejected.decisionReason() == "Доказательства не подтверждены");
    REQUIRE_THROWS_AS(rejected.assess(registry.at(id), Money(800)), PaymentException);
    claim.approve(registry.at(id), test::branch(), "worker", Money(500), Money(800),
                  test::moment(20));
    REQUIRE(claim != rejected);
    REQUIRE_THROWS_AS(claim.approve(registry.at(id), test::branch(), "worker", Money(500),
                                    Money(800), test::moment(21)),
                      PaymentException);
}
TEST_CASE("CompensationClaim invalid submissions leave the draft untouched",
          "[compensation-claim]") {
    const auto registry = test::atStatus(ShipmentStatus::Accepted);
    const auto& shipment = registry.at("BY-1");
    CompensationClaim claim("c");
    const auto before = claim;
    REQUIRE_THROWS_AS(claim.submit(shipment, test::customer("stranger"), Money(10),
                                   CompensationReason::Damaged, {"p"}, test::moment(10)),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(claim.submit(shipment, shipment.sender(), Money(0),
                                   CompensationReason::Damaged, {"p"}, test::moment(10)),
                      PaymentException);
    REQUIRE_THROWS_AS(claim.submit(shipment, shipment.sender(), Money(10),
                                   CompensationReason::Damaged, {}, test::moment(10)),
                      PaymentException);
    REQUIRE_THROWS_AS(claim.submit(shipment, shipment.sender(), Money(10),
                                   CompensationReason::Damaged, {"p", "p"}, test::moment(10)),
                      PaymentException);
    REQUIRE_THROWS_AS(claim.submit(shipment, shipment.sender(), Money(10), CompensationReason::Lost,
                                   {"p"}, test::moment(10)),
                      PaymentException);
    REQUIRE(claim == before);
}
