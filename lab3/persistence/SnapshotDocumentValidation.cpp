#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
using namespace snapshot;
void SnapshotCodec::validatePayments(const StoreState& state) {
    std::set<std::string> paymentOrders;
    std::set<std::string> indexedAttempts;
    std::set<std::string> indexedRefunds;
    for (const auto& [id, payment] : state.payments_.payments()) {
        require(paymentOrders.insert(payment.orderId()).second, "Повторная оплата одного заказа");
        require(payment.capturedAt() <= state.now(), "Оплата в будущем");
        payment.refundedTotal(state.payments_.refunds());
        checkCounter(id, "payment", state.nextSequence_);
    }
    for (const auto& [id, order] : state.orders_.orders()) {
        require(state.payments_.reconcileOrderBalance(*order).empty(),
                "Баланс оплаты/возвратов не согласован с заказом");
    }
    for (const auto& [id, attempt] : state.payments_.attempts()) {
        const auto order = state.orders_.order(attempt.orderId());
        require(attempt.createdAt() >= order->createdAt() &&
                    attempt.createdAt() < order->paymentDeadline() &&
                    attempt.createdAt() <= state.now(),
                "Попытка оплаты вне срока покупки");
        // После отказа в оплате доставка может измениться. Сравниваем с условиями на момент
        // попытки.
        Money expected = order->goodsNet();
        auto delivery = std::get<InitialOrderTerms>(order->history().front().payload()).delivery;
        for (const auto& event : order->history()) {
            if (event.timestamp() > attempt.createdAt()) {
                break;
            }
            if (event.kind() == OrderEventKind::DeliveryChanged) {
                delivery = std::get<DeliveryChange>(event.payload()).after;
            }
        }
        expected = expected.checkedAdd(delivery.fee);
        // По одинаковым отметкам времени нельзя определить порядок отказа и изменения доставки.
        const bool editedAtSameTime = std::any_of(
            order->history().begin(), order->history().end(), [&attempt](const auto& event) {
                return event.kind() == OrderEventKind::DeliveryChanged &&
                       event.timestamp() == attempt.createdAt();
            });
        require(editedAtSameTime || attempt.amount() == expected,
                "Сумма попытки не соответствует условиям покупки");
        const auto payment = state.payments_.findPaymentForOrder(order->id());
        if (attempt.status() == AttemptStatus::Succeeded) {
            require(payment && payment->attemptId() == id,
                    "Успешная попытка без соответствующей оплаты");
        } else {
            require(!payment || attempt.createdAt() <= payment->capturedAt(),
                    "Отказ оплаты после окончательной оплаты");
        }
        checkCounter(id, "attempt", state.nextSequence_);
    }
    for (const auto& [id, refund] : state.payments_.refunds()) {
        const auto payment = state.payments_.payment(refund.paymentId());
        require(payment.orderId() == refund.orderId() && payment.refundIds().contains(id) &&
                    refund.createdAt() <= state.now(),
                "Неверная ссылка возврата денег");
        const auto& operation = detail::get(state.payments_.operationIndex(), refund.operationId());
        const auto* typed = std::get_if<RefundOperation>(&operation);
        const auto& global = detail::get(state.operations_, refund.operationId());
        require(global.signature.kind == (refund.returnRequestId() ? CommandKind::CompleteReturn
                                                                   : CommandKind::Cancel) &&
                    global.signature.entityId ==
                        (refund.returnRequestId() ? *refund.returnRequestId() : refund.orderId()),
                "Возврат денег не соответствует предметной команде");
        require(typed && typed->resultId == id && typed->paymentId == payment.id() &&
                    typed->amount == refund.amount() &&
                    typed->returnRequestId == refund.returnRequestId(),
                "Возврат денег не совпадает с журналом");
        checkCounter(id, "refund", state.nextSequence_);
    }
    for (const auto& [id, operation] : state.payments_.operationIndex()) {
        if (const auto* attempt = std::get_if<AttemptOperation>(&operation)) {
            const auto& document =
                detail::get(state.payments_.attempts(), attempt->result.attemptId);
            const auto payment = state.payments_.findPaymentForOrder(attempt->orderId);
            require(indexedAttempts.insert(document.id()).second &&
                        attempt->orderId == document.orderId() &&
                        attempt->amount == document.amount() &&
                        attempt->method == document.method() &&
                        attempt->failureReason == document.failureReason() &&
                        attempt->accepted == (document.status() == AttemptStatus::Succeeded) &&
                        attempt->result.successful == attempt->accepted &&
                        (attempt->accepted ? payment && attempt->result.paymentId == payment->id()
                                           : !attempt->result.paymentId),
                    "Попытка не совпадает с финансовой операцией");
            const auto& global = detail::get(state.operations_, id);
            require(global.signature.kind == CommandKind::Pay &&
                        global.signature.entityId == attempt->orderId &&
                        global.signature.texts ==
                            std::vector<std::string>{attempt->failureReason} &&
                        global.signature.numbers ==
                            std::vector<std::int64_t>{attempt->amount.kopecks(),
                                                      static_cast<std::int64_t>(attempt->method),
                                                      attempt->accepted} &&
                        global.result == OperationResult(attempt->result),
                    "Финансовая операция не совпадает с командой оплаты");
        } else {
            const auto& refund = std::get<RefundOperation>(operation);
            const auto& document = detail::get(state.payments_.refunds(), refund.resultId);
            require(indexedRefunds.insert(document.id()).second && document.operationId() == id &&
                        document.paymentId() == refund.paymentId &&
                        document.amount() == refund.amount &&
                        document.returnRequestId() == refund.returnRequestId,
                    "Возврат денег не совпадает с финансовой операцией");
        }
    }
    require(indexedAttempts.size() == state.payments_.attempts().size() &&
                indexedRefunds.size() == state.payments_.refunds().size(),
            "Финансовые документы без защиты повторов");
}
void SnapshotCodec::validateReturns(const StoreState& state) {
    std::set<std::string> activeOrders;
    std::set<std::string> referencedInspections;
    std::set<std::string> completedRequests;
    for (const auto& [orderId, order] : state.orders_.orders()) {
        std::map<std::string, std::int64_t> completed;
        for (const auto& event : order->history()) {
            if (event.kind() != OrderEventKind::ReturnCompleted) {
                continue;
            }
            const auto& request = detail::get(state.returnRequests_, event.relatedEntityId());
            require(request.status() == ReturnStatus::Completed && request.orderId() == orderId &&
                        request.inspectionId() && completedRequests.insert(request.id()).second,
                    "Событие завершения не соответствует заявке");
            const auto& inspection = detail::get(state.inspections_, *request.inspectionId());
            require(event.timestamp() >= inspection.receivedAt(),
                    "Завершение возврата раньше осмотра");
            for (const auto& line : request.lines()) {
                require(completed[line.sku()] == line.alreadyReturnedOffset(),
                        "Неверное смещение последовательного возврата");
                completed[line.sku()] = detail::addNonnegative(
                    completed[line.sku()],
                    detail::get(inspection.results(), line.sku()).acceptedQuantity());
                require(completed[line.sku()] <= order->line(line.sku()).quantity(),
                        "Возвращено больше купленного");
            }
            const auto amount = requestAmount(request, inspection, *order);
            if (amount == Money{}) {
                require(!request.refundId(), "Денежный документ нулевого возврата");
            } else {
                require(request.refundId().has_value(),
                        "Нет денежного документа завершённого возврата");
                const auto& refund = detail::get(state.payments_.refunds(), *request.refundId());
                require(refund.returnRequestId() == request.id() && refund.orderId() == orderId &&
                            refund.amount() == amount && refund.createdAt() == event.timestamp() &&
                            refund.operationId() == event.operationId(),
                        "Деньги не совпадают с завершением возврата");
            }
        }
        for (const auto& [requestId, request] : state.returnRequests_) {
            if (request.orderId() != orderId || !request.active()) {
                continue;
            }
            for (const auto& line : request.lines()) {
                require(line.alreadyReturnedOffset() == completed[line.sku()],
                        "Активная заявка использует устаревшее возвратное смещение");
            }
        }
    }
    for (const auto& [id, request] : state.returnRequests_) {
        const auto order = state.orders_.order(request.orderId());
        require(request.customerId() == order->customerId() && request.createdAt() <= state.now(),
                "Неверный владелец или время возврата");
        if (request.active()) {
            require(activeOrders.insert(order->id()).second, "Две активные заявки одного заказа");
        }
        require((request.status() == ReturnStatus::Completed) == completedRequests.contains(id),
                "Завершённая заявка без события");
        if (request.inspectionId()) {
            const auto& inspection = detail::get(state.inspections_, *request.inspectionId());
            require(referencedInspections.insert(inspection.id()).second &&
                        inspection.requestId() == id && inspection.receivedAt() <= state.now(),
                    "Неверный или повторный осмотр заявки");
            const bool accepted =
                std::any_of(inspection.results().begin(), inspection.results().end(),
                            [](const auto& pair) { return pair.second.acceptedQuantity() > 0; });
            require(accepted || request.status() == ReturnStatus::Rejected,
                    "Пустой результат осмотра должен быть отказом");
        }
        checkCounter(id, "return", state.nextSequence_);
    }
    require(referencedInspections.size() == state.inspections_.size(),
            "Осмотр без связанной заявки");
    for (const auto& [id, inspection] : state.inspections_) {
        checkCounter(id, "inspection", state.nextSequence_);
    }
    for (const auto& [id, refund] : state.payments_.refunds()) {
        if (refund.returnRequestId()) {
            const auto& request = detail::get(state.returnRequests_, *refund.returnRequestId());
            require(request.status() == ReturnStatus::Completed && request.refundId() == id,
                    "Деньги возвращены по незавершённой заявке");
        }
    }
}
} // namespace shop
