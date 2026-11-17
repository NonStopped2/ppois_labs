#include "../PurchaseFixtures.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/ReturnRuleException.hpp"
#include "shop/returns/ReturnInspection.hpp"
#include "shop/returns/ReturnRequest.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

TEST_CASE("Return lines freeze the next exact refundable unit range") {
    const auto order = test::deliveredOrder();
    const ReturnLine first(order.line("shirt-blue"), 1, 0, ReturnReason::ChangedMind);
    REQUIRE(first.sku() == "shirt-blue");
    REQUIRE(first.requestedQuantity() == 1);
    REQUIRE(first.alreadyReturnedOffset() == 0);
    REQUIRE(first.reason() == ReturnReason::ChangedMind);
    REQUIRE(first.maximumRefund() == Money(34));
    REQUIRE(ReturnLine(order.line("shirt-blue"), 2, 1, ReturnReason::Defective).maximumRefund() ==
            Money(66));
    REQUIRE_THROWS_AS(ReturnLine(order.line("shirt-blue"), 1, 3, ReturnReason::ChangedMind),
                      ReturnRuleException);
    REQUIRE_THROWS_AS(ReturnLine(order.line("shirt-blue"), 0, 0, ReturnReason::ChangedMind),
                      ReturnRuleException);
    REQUIRE_THROWS_AS(ReturnLine(order.line("shirt-blue"), 1, 0, static_cast<ReturnReason>(99)),
                      ReturnRuleException);
}

TEST_CASE("Inspection cannot finalize incomplete receipt or restock damaged goods") {
    const auto order = test::deliveredOrder();
    ReturnRequest request(
        "r", order, {ReturnLine(order.line("shirt-blue"), 2, 0, ReturnReason::ChangedMind)}, 15);
    ReturnInspection inspection("i", request, 16);
    REQUIRE(inspection.id() == "i");
    REQUIRE(inspection.requestId() == "r");
    REQUIRE(inspection.receivedAt() == 16);
    REQUIRE_FALSE(inspection.finalized());
    REQUIRE_THROWS_AS(inspection.finalize(request), ReturnRuleException);
    inspection.recordResult(InspectionResult("shirt-blue", 1, 1, 1, ItemCondition::Opened), request,
                            order.returnPolicy());
    REQUIRE_THROWS_AS(inspection.finalize(request), ReturnRuleException);
    REQUIRE_THROWS_AS(
        inspection.recordResult(InspectionResult("shirt-blue", 1, 1, 1, ItemCondition::Opened),
                                request, order.returnPolicy()),
        DuplicateEntityException);
    ReturnInspection complete("i2", request, 16);
    const InspectionResult result("shirt-blue", 2, 1, 0, ItemCondition::Defective);
    REQUIRE(result.sku() == "shirt-blue");
    REQUIRE(result.receivedQuantity() == 2);
    REQUIRE(result.acceptedQuantity() == 1);
    REQUIRE(result.restockQuantity() == 0);
    REQUIRE(result.condition() == ItemCondition::Defective);
    complete.recordResult(result, request, order.returnPolicy());
    complete.finalize(request);
    REQUIRE(complete.results().size() == 1);
    REQUIRE(complete.finalized());
    REQUIRE_THROWS_AS(complete.finalize(request), ReturnRuleException);
    REQUIRE_THROWS_AS(complete.recordResult(result, request, order.returnPolicy()),
                      ReturnRuleException);
    REQUIRE_THROWS_AS(InspectionResult("shirt-blue", 1, 1, 1, ItemCondition::Defective),
                      ReturnRuleException);
    REQUIRE_THROWS_AS(InspectionResult("shirt-blue", 1, 2, 0, ItemCondition::Opened),
                      ReturnRuleException);
    REQUIRE_THROWS_AS(InspectionResult("shirt-blue", 1, 1, -1, ItemCondition::Opened),
                      ReturnRuleException);
    REQUIRE(order.returnPolicy().calculateRefund(order, request, complete, {}) == Money(34));
    REQUIRE_THROWS_AS(
        order.returnPolicy().calculateRefund(order, request, complete, {{"shirt-blue", 1}}),
        ReturnRuleException);
}

TEST_CASE("Return request phases keep the original deadline through amendments and inspection") {
    const auto order = test::deliveredOrder();
    const ReturnLine line(order.line("shirt-blue"), 1, 0, ReturnReason::ChangedMind);
    ReturnRequest request("r", order, {line}, 15);
    REQUIRE(request.id() == "r");
    REQUIRE(request.orderId() == "order");
    REQUIRE(request.customerId() == "alice");
    REQUIRE(request.createdAt() == 15);
    REQUIRE(request.deadline() == 114);
    REQUIRE(request.status() == ReturnStatus::Submitted);
    REQUIRE(request.active());
    REQUIRE(request.lines().size() == 1);
    REQUIRE_FALSE(request.inspectionId());
    REQUIRE_FALSE(request.refundId());
    REQUIRE(request.decisionReason().empty());
    REQUIRE(request.maximumRefund() == Money(34));
    REQUIRE(request.line("shirt-blue") == line);
    const auto before = request;
    REQUIRE_THROWS_AS(request.amend({}, order, 16), ReturnRuleException);
    REQUIRE(request == before);
    request.amend({ReturnLine(order.line("shirt-blue"), 2, 0, ReturnReason::Defective)}, order, 20);
    REQUIRE(request.maximumRefund() == Money(67));
    REQUIRE(request.deadline() == 114);
    auto withdrawn = request;
    withdrawn.withdraw("alice", 21);
    REQUIRE(withdrawn.status() == ReturnStatus::Withdrawn);
    REQUIRE_FALSE(withdrawn.active());
    REQUIRE_THROWS_AS(request.withdraw("bob", 21), ReturnRuleException);
    auto rejected = request;
    rejected.reject("Причина", 21);
    REQUIRE(rejected.status() == ReturnStatus::Rejected);
    REQUIRE(rejected.decisionReason() == "Причина");
    ReturnInspection inspection("i", request, 21);
    inspection.recordResult(InspectionResult("shirt-blue", 2, 2, 0, ItemCondition::Defective),
                            request, order.returnPolicy());
    inspection.finalize(request);
    request.attachInspection(inspection);
    REQUIRE(request.status() == ReturnStatus::Inspected);
    REQUIRE(request.inspectionId() == "i");
    REQUIRE_THROWS_AS(request.withdraw("alice", 22), ReturnRuleException);
    REQUIRE_THROWS_AS(request.amend({line}, order, 22), ReturnRuleException);
    request.complete("refund");
    REQUIRE(request.status() == ReturnStatus::Completed);
    REQUIRE(request.refundId() == "refund");
    REQUIRE_THROWS_AS(request.complete("again"), ReturnRuleException);
    REQUIRE_THROWS_AS(ReturnRequest("bad", test::paidOrder(), {line}, 15), ReturnRuleException);
    REQUIRE_THROWS_AS(ReturnRequest("bad", order, {line, line}, 15), DuplicateEntityException);
}
