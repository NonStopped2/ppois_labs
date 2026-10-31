#include "../BranchFixtures.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace post;
TEST_CASE("ReturnRequest coordinates full return to sender without swapping customer snapshots",
          "[return-request]") {
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id);
    auto outgoing = test::preparedBatch(registry, target);
    source.transferOut(registry, outgoing, test::sortingPlan(), "worker", test::moment(30));
    target.receiveBatch(registry, outgoing, "worker", test::storagePolicy(), test::moment(2200));
    const auto sender = registry.at(id).sender(), recipient = registry.at(id).recipient();
    ReturnRequest request(" request ");
    REQUIRE(request.id() == "request");
    REQUIRE(request.status() == ReturnStatus::Draft);
    REQUIRE(request.shipmentId().empty());
    REQUIRE_FALSE(request.requestedAt());
    REQUIRE_FALSE(request.decidedAt());
    REQUIRE(request.decisionReason().empty());
    request.submit(registry.at(id), recipient, ReturnReason::RecipientRefused, test::moment(2250));
    REQUIRE(request.shipmentId() == id);
    REQUIRE(request.initiatorId() == recipient.id());
    REQUIRE(request.initiatorKind() == ReturnInitiatorKind::Customer);
    REQUIRE(request.reason() == ReturnReason::RecipientRefused);
    REQUIRE(request.requestedAt() == test::moment(2250));
    request.approve(registry.at(id), test::employee("worker", "B"), target, test::storagePolicy(),
                    test::moment(2260));
    REQUIRE(request.status() == ReturnStatus::Approved);
    REQUIRE(request.decidedAt() == test::moment(2260));
    REQUIRE_FALSE(request.decisionReason().empty());
    const auto before = request;
    REQUIRE_THROWS_AS(
        request.beginReturn(target, registry, test::employee("worker", "B"), test::moment(2259)),
        InvalidShipmentStateException);
    REQUIRE(request == before);
    request.beginReturn(target, registry, test::employee("worker", "B"), test::moment(2270));
    REQUIRE(request.status() == ReturnStatus::Started);
    REQUIRE(registry.at(id).status() == ShipmentStatus::Returning);
    REQUIRE(target.storageEntry(id).shipmentId() == id);
    auto returning = test::preparedBatch(registry, source, {id}, "B", 2300);
    target.transferOut(registry, returning, test::sortingPlan("B", "A"), "worker",
                       test::moment(2310));
    source.receiveBatch(registry, returning, "worker", test::storagePolicy(), test::moment(4510));
    REQUIRE(registry.at(id).status() == ShipmentStatus::Returning);
    source.issueShipment(registry, id, "worker", sender, test::document(), test::moment(4520));
    REQUIRE(registry.at(id).status() == ShipmentStatus::Returned);
    REQUIRE(registry.at(id).sender() == sender);
    REQUIRE(registry.at(id).recipient() == recipient);
    REQUIRE(registry.at(id).history().timeSpentAt("A", test::moment(9000)) ==
            std::chrono::seconds{25});
    REQUIRE(registry.archiveCompletedBefore(test::moment(4521)) == 1);
    REQUIRE(registry.archivedCount() == 1);
}
TEST_CASE("ReturnRequest rejects foreign initiator wrong reason repeated submission and movement",
          "[return-request]") {
    ShipmentRegistry registry("BY");
    auto branch = test::branch();
    const auto id = test::registerCargo(registry);
    test::accept(branch, registry, id);
    ReturnRequest request("r");
    const auto before = request;
    REQUIRE_THROWS_AS(request.submit(registry.at(id), test::customer("stranger"),
                                     ReturnReason::SenderRequest, test::moment(16)),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(request.submit(registry.at(id), registry.at(id).recipient(),
                                     ReturnReason::SenderRequest, test::moment(16)),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(request.submit(registry.at(id), registry.at(id).sender(),
                                     ReturnReason::StorageExpired, test::moment(16)),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(request.submit(registry.at(id), test::employee(), branch,
                                     ReturnReason::SenderRequest, test::moment(16)),
                      DeliveryAuthorizationException);
    REQUIRE_THROWS_AS(request.submit(registry.at(id), registry.at(id).sender(),
                                     ReturnReason::SenderRequest, test::moment(14)),
                      InvalidShipmentStateException);
    REQUIRE(request == before);
    request.submit(registry.at(id), registry.at(id).sender(), ReturnReason::SenderRequest,
                   test::moment(16));
    REQUIRE_THROWS_AS(request.submit(registry.at(id), registry.at(id).sender(),
                                     ReturnReason::SenderRequest, test::moment(17)),
                      InvalidShipmentStateException);
    for (const auto state : {ShipmentStatus::Draft, ShipmentStatus::InTransit,
                             ShipmentStatus::Delivered, ShipmentStatus::Returning}) {
        const auto moved = test::atStatus(state);
        ReturnRequest other("other");
        REQUIRE_THROWS_AS(other.submit(moved.at("BY-1"), moved.at("BY-1").sender(),
                                       ReturnReason::SenderRequest, test::moment(100)),
                          InvalidShipmentStateException);
    }
}
TEST_CASE("ReturnRequest rejects with explanation and cannot decide or start twice",
          "[return-request]") {
    ShipmentRegistry registry("BY");
    auto branch = test::branch();
    const auto id = test::registerCargo(registry);
    test::accept(branch, registry, id);
    ReturnRequest request("r");
    request.submit(registry.at(id), registry.at(id).sender(), ReturnReason::SenderRequest,
                   test::moment(16));
    const auto before = request;
    REQUIRE_THROWS_AS(
        request.reject(registry.at(id), test::employee(), branch, " ", test::moment(17)),
        std::invalid_argument);
    REQUIRE_THROWS_AS(request.approve(registry.at(id), test::employee(), branch,
                                      test::storagePolicy(), test::moment(15)),
                      InvalidShipmentStateException);
    REQUIRE(request == before);
    request.reject(registry.at(id), test::employee(), branch, " Нет  основания ", test::moment(17));
    REQUIRE(request.status() == ReturnStatus::Rejected);
    REQUIRE(request.decisionReason() == "Нет основания");
    REQUIRE(request.decidedAt() == test::moment(17));
    REQUIRE(registry.at(id).status() == ShipmentStatus::Accepted);
    REQUIRE_THROWS_AS(request.approve(registry.at(id), test::employee(), branch,
                                      test::storagePolicy(), test::moment(18)),
                      InvalidShipmentStateException);
    REQUIRE_THROWS_AS(request.beginReturn(branch, registry, test::employee(), test::moment(18)),
                      InvalidShipmentStateException);
    REQUIRE(before != request);
    REQUIRE(before.status() == ReturnStatus::Submitted);
}
TEST_CASE("ReturnRequest storage expiry uses inclusive saved deadline and late active shift",
          "[return-request]") {
    const StoragePolicy policy("short", 0, 1, Money(50), 1, 0);
    ShipmentRegistry registry("BY");
    auto branch = test::branch();
    const auto id = test::registerCargo(registry);
    test::accept(branch, registry, id, policy);
    ReturnRequest request("r");
    request.submit(registry.at(id), test::employee(), branch, ReturnReason::StorageExpired,
                   test::moment(16));
    REQUIRE(request.initiatorKind() == ReturnInitiatorKind::Employee);
    const auto before = request;
    REQUIRE_THROWS_AS(
        request.approve(registry.at(id), test::employee(), branch, policy, test::moment(17)),
        InvalidShipmentStateException);
    REQUIRE(request == before);
    const auto deadline = branch.storageEntry(id).deadlineAt();
    branch.addShift(WorkShift("late", "worker", "A", deadline - std::chrono::seconds{10},
                              deadline + std::chrono::seconds{10}));
    branch.startShift("late", deadline - std::chrono::seconds{10});
    REQUIRE_THROWS_AS(request.approve(registry.at(id), test::employee(), branch, policy,
                                      deadline - std::chrono::seconds{1}),
                      InvalidShipmentStateException);
    request.approve(registry.at(id), test::employee(), branch, policy, deadline);
    request.beginReturn(branch, registry, test::employee(), deadline);
    branch.issueShipment(registry, id, "worker", registry.at(id).sender(), test::document(),
                         deadline + std::chrono::seconds{1});
    REQUIRE(registry.at(id).status() == ShipmentStatus::Returned);
}
TEST_CASE("ReturnRequest does not reuse expiry approval after a new storage placement",
          "[return-request]") {
    const StoragePolicy shortPolicy("short", 0, 1, Money(50), 1, 0);
    ShipmentRegistry registry("BY");
    auto source = test::branch();
    auto target = test::branch("B", "23");
    const auto id = test::registerCargo(registry);
    test::accept(source, registry, id, shortPolicy);
    ReturnRequest request("r");
    request.submit(registry.at(id), test::employee(), source, ReturnReason::StorageExpired,
                   test::moment(16));
    const auto deadline = source.storageEntry(id).deadlineAt();
    for (auto* branch : {&source, &target}) {
        branch->addShift(WorkShift("late", "worker", branch->id(),
                                   deadline - std::chrono::seconds{10},
                                   deadline + std::chrono::seconds{10000}));
        branch->startShift("late", deadline - std::chrono::seconds{10});
    }
    request.approve(registry.at(id), test::employee(), source, shortPolicy, deadline);
    auto batch =
        test::preparedBatch(registry, target, {id}, "A", deadline.time_since_epoch().count() + 1);
    source.transferOut(registry, batch, test::sortingPlan(), "worker",
                       deadline + std::chrono::seconds{2});
    target.receiveBatch(registry, batch, "worker", test::storagePolicy(),
                        deadline + std::chrono::seconds{2200});
    const auto before = request;
    const auto targetBefore = target;
    const auto history = registry.at(id).history();
    REQUIRE_THROWS_AS(request.beginReturn(target, registry, test::employee("worker", "B"),
                                          deadline + std::chrono::seconds{2201}),
                      InvalidShipmentStateException);
    REQUIRE(request == before);
    REQUIRE(target == targetBefore);
    REQUIRE(registry.at(id).history() == history);
}

TEST_CASE("ReturnRequest revalidates authority location and shipment before beginning",
          "[return-request]") {
    ShipmentRegistry registry("BY");
    auto branch = test::branch();
    const auto id = test::registerCargo(registry);
    test::accept(branch, registry, id);
    ReturnRequest request("r");
    request.submit(registry.at(id), registry.at(id).sender(), ReturnReason::SenderRequest,
                   test::moment(16));
    REQUIRE_THROWS_AS(request.approve(registry.at(id), test::employee("worker", "B"), branch,
                                      test::storagePolicy(), test::moment(17)),
                      DeliveryAuthorizationException);
    request.approve(registry.at(id), test::employee(), branch, test::storagePolicy(),
                    test::moment(17));
    detail::appendFact(registry.at(id), TrackingEventKind::Lost, test::moment(18), "A", "Утрата");
    const auto before = request;
    const auto branchBefore = branch;
    const auto history = registry.at(id).history();
    REQUIRE_THROWS_AS(request.beginReturn(branch, registry, test::employee(), test::moment(19)),
                      InvalidShipmentStateException);
    REQUIRE(request == before);
    REQUIRE(branch == branchBefore);
    REQUIRE(registry.at(id).history() == history);
}
