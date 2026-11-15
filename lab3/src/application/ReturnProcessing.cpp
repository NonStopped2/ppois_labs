#include "shop/application/ReturnProcessing.hpp"
#include "shop/application/PreparedChange.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/PaymentRuleException.hpp"
#include "shop/common/ReturnRuleException.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>
#include <numeric>
#include <set>

namespace shop {
namespace {
CommandSignature signatureFor(CommandKind kind, std::string_view actor, std::string_view entity,
                              std::vector<ReturnSelection> selections = {},
                              std::vector<InspectionResult> results = {},
                              std::string_view reason = {}) {
    std::sort(selections.begin(), selections.end(),
              [](const auto& left, const auto& right) { return left.sku < right.sku; });
    std::sort(results.begin(), results.end(),
              [](const auto& left, const auto& right) { return left.sku() < right.sku(); });
    return {kind,         std::string(actor), std::string(entity),   {std::string(reason)}, {},
            std::nullopt, std::nullopt,       std::move(selections), std::move(results)};
}
} // namespace
ReturnProcessing::ReturnProcessing(StoreState& state) : state_(state) {}
std::map<std::string, std::int64_t>
ReturnProcessing::completedQuantities(std::string_view orderId) const {
    const auto order = state_.orders_.order(orderId);
    std::map<std::string, std::int64_t> result;
    for (const auto& event : order->history()) {
        if (event.kind() != OrderEventKind::ReturnCompleted) {
            continue;
        }
        const auto& request = detail::get(state_.returnRequests_, event.relatedEntityId());
        detail::check<ReturnRuleException>(request.status() == ReturnStatus::Completed &&
                                               request.inspectionId().has_value(),
                                           "Событие возврата не согласовано с заявкой");
        const auto& inspection = detail::get(state_.inspections_, *request.inspectionId());
        for (const auto& [sku, line] : inspection.results()) {
            result[sku] = detail::addNonnegative(result[sku], line.acceptedQuantity());
        }
    }
    return result;
}
std::vector<ReturnLine>
ReturnProcessing::buildLines(const Order& order,
                             const std::vector<ReturnSelection>& selections) const {
    detail::check<ReturnRuleException>(order.status() == OrderStatus::Delivered &&
                                           !selections.empty(),
                                       "Требуется полученный заказ и непустой состав возврата");
    const auto completed = completedQuantities(order.id());
    std::set<std::string> seen;
    std::vector<ReturnLine> result;
    for (const auto& entry : selections) {
        detail::check<DuplicateEntityException>(seen.insert(entry.sku).second,
                                                "Повторный SKU заявки");
        const auto purchased = order.line(entry.sku);
        order.returnPolicy().assertEligible(purchased.returnEligible(), *order.deliveredAt(),
                                            entry.reason, state_.now());
        const auto prior = completed.find(entry.sku);
        result.emplace_back(purchased, entry.quantity, prior == completed.end() ? 0 : prior->second,
                            entry.reason);
    }
    return result;
}
std::string ReturnProcessing::open(std::string_view actor, std::string_view orderId,
                                   const std::vector<ReturnSelection>& selections,
                                   std::string_view operationId) {
    const auto signature = signatureFor(CommandKind::OpenReturn, actor, orderId, selections);
    if (const auto replay = replayOperation(state_.operations_, operationId, signature)) {
        return std::get<std::string>(*replay);
    }
    const auto order = state_.orders_.order(orderId);
    order->assertOwner(actor);
    for (const auto& [id, request] : state_.returnRequests_) {
        detail::check<ReturnRuleException>(
            request.orderId() != orderId || !request.active(),
            "У заказа уже есть активная заявка; истёкшие заявки закрываются отдельной командой");
    }
    auto lines = buildLines(*order, signature.returnSelections);
    auto id = state_.nextId("return");
    const ReturnRequest request(id, *order, std::move(lines), state_.now());
    PreparedChange change(state_);
    change.returns().emplace(id, request);
    recordOperation(change.operations(), operationId, signature, id);
    change.commit();
    return id;
}
std::string ReturnProcessing::inspect(std::string_view requestId,
                                      const std::vector<InspectionResult>& results,
                                      std::string_view operationId) {
    const auto signature =
        signatureFor(CommandKind::InspectReturn, "operator", requestId, {}, results);
    if (const auto replay = replayOperation(state_.operations_, operationId, signature)) {
        return std::get<std::string>(*replay);
    }
    auto request = detail::get(state_.returnRequests_, requestId);
    const auto order = state_.orders_.order(request.orderId());
    auto id = state_.nextId("inspection");
    ReturnInspection inspection(id, request, state_.now());
    for (const auto& result : signature.inspectionResults) {
        inspection.recordResult(result, request, order->returnPolicy());
    }
    inspection.finalize(request);
    request.attachInspection(inspection);
    const bool noneAccepted =
        std::all_of(inspection.results().begin(), inspection.results().end(),
                    [](const auto& pair) { return pair.second.acceptedQuantity() == 0; });
    if (noneAccepted) {
        request.reject("По результатам осмотра нет принятых единиц", state_.now());
    }
    PreparedChange change(state_);
    change.inspections().emplace(id, inspection);
    change.returns().at(request.id()) = std::move(request);
    recordOperation(change.operations(), operationId, signature, id);
    change.commit();
    return id;
}
void ReturnProcessing::reject(std::string_view requestId, std::string_view reason,
                              std::string_view operationId) {
    const auto explanation = detail::text(reason, "причина отказа возврата");
    const auto signature =
        signatureFor(CommandKind::RejectReturn, "operator", requestId, {}, {}, explanation);
    if (replayOperation(state_.operations_, operationId, signature)) {
        return;
    }
    auto request = detail::get(state_.returnRequests_, requestId);
    request.reject(explanation, state_.now());
    PreparedChange change(state_);
    change.returns().at(request.id()) = std::move(request);
    recordOperation(change.operations(), operationId, signature);
    change.commit();
}
void ReturnProcessing::withdraw(std::string_view actor, std::string_view requestId,
                                std::string_view operationId) {
    const auto signature = signatureFor(CommandKind::WithdrawReturn, actor, requestId);
    if (replayOperation(state_.operations_, operationId, signature)) {
        return;
    }
    auto request = detail::get(state_.returnRequests_, requestId);
    request.withdraw(actor, state_.now());
    PreparedChange change(state_);
    change.returns().at(request.id()) = std::move(request);
    recordOperation(change.operations(), operationId, signature);
    change.commit();
}
Money ReturnProcessing::complete(std::string_view requestId, std::string_view operationId) {
    const auto signature = signatureFor(CommandKind::CompleteReturn, "operator", requestId);
    if (const auto replay = replayOperation(state_.operations_, operationId, signature)) {
        return std::get<Money>(*replay);
    }
    auto request = detail::get(state_.returnRequests_, requestId);
    detail::check<ReturnRuleException>(request.status() == ReturnStatus::Inspected &&
                                           request.inspectionId().has_value(),
                                       "Завершение требует принятого осмотра");
    const auto& inspection = detail::get(state_.inspections_, *request.inspectionId());
    auto order = *state_.orders_.order(request.orderId());
    const auto amount = order.returnPolicy().calculateRefund(order, request, inspection,
                                                             completedQuantities(order.id()));
    std::map<std::string, std::int64_t> restock;
    for (const auto& [sku, result] : inspection.results()) {
        if (result.restockQuantity() > 0) {
            restock.emplace(sku, result.restockQuantity());
        }
    }
    PreparedChange change(state_);
    std::optional<std::string> refundId;
    if (amount > Money{}) {
        const auto payment = state_.payments_.findPaymentForOrder(order.id());
        detail::check<PaymentRuleException>(payment.has_value(),
                                            "Возврату не соответствует оплата");
        refundId = state_.nextId("refund");
        change.payments().refund(*refundId, operationId, payment->id(), request.id(), amount,
                                 state_.now());
    }
    if (!restock.empty()) {
        change.inventory().restock(restock);
    }
    change.loyalty().at(order.customerId()).recordReturn(order.id(), amount, operationId);
    request.complete(refundId);
    order.recordReturnCompleted(state_.now(), operationId, request.id());
    change.returns().at(request.id()) = std::move(request);
    change.orders().replace(order);
    recordOperation(change.operations(), operationId, signature, amount);
    change.commit();
    return amount;
}
void ReturnProcessing::amendSubmitted(std::string_view actor, std::string_view requestId,
                                      const std::vector<ReturnSelection>& selections,
                                      std::string_view operationId) {
    const auto signature = signatureFor(CommandKind::AmendReturn, actor, requestId, selections);
    if (replayOperation(state_.operations_, operationId, signature)) {
        return;
    }
    auto request = detail::get(state_.returnRequests_, requestId);
    request.assertOwner(actor);
    const auto order = state_.orders_.order(request.orderId());
    request.amend(buildLines(*order, signature.returnSelections), *order, state_.now());
    PreparedChange change(state_);
    change.returns().at(request.id()) = std::move(request);
    recordOperation(change.operations(), operationId, signature);
    change.commit();
}
std::vector<std::string> ReturnProcessing::closeExpiredUnreceived() {
    std::vector<std::string> closed;
    for (const auto& [id, request] : state_.returnRequests_) {
        if (request.status() == ReturnStatus::Submitted && state_.now() >= request.deadline()) {
            closed.push_back(id);
        }
    }
    for (const auto& id : closed) {
        const auto signature = signatureFor(CommandKind::ExpireReturn, "system", id);
        const auto operation = "system-expire-return-" + id;
        auto request = detail::get(state_.returnRequests_, id);
        request.reject("NotReceivedInTime", state_.now());
        PreparedChange change(state_);
        change.returns().at(id) = std::move(request);
        recordOperation(change.operations(), operation, signature);
        change.commit();
    }
    return closed;
}
} // namespace shop
