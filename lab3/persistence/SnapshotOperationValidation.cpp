#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
using namespace snapshot;
namespace {
const OrderEvent& matchingEvent(const Order& order, OrderEventKind kind,
                                const std::string& operation) {
    const auto found =
        std::find_if(order.history().begin(), order.history().end(), [&](const auto& event) {
            return event.kind() == kind && event.operationId() == operation;
        });
    require(found != order.history().end(),
            "Команда не соответствует событию заказа: " + operation);
    return *found;
}
void shape(const CommandSignature& command, std::size_t texts, std::size_t numbers) {
    require(command.texts.size() == texts && command.numbers.size() == numbers,
            "Неверный состав параметров сохранённой команды");
}
void emptyResult(const OperationResult& result) {
    require(std::holds_alternative<std::monostate>(result), "Лишний результат команды");
}
const std::string& stringResult(const OperationResult& result) {
    const auto* saved = std::get_if<std::string>(&result);
    require(saved, "Неверный тип результата команды");
    return *saved;
}
} // namespace
void SnapshotCodec::validateOperations(const StoreState& state) {
    std::set<std::string> placedOrders;
    std::set<std::string> openedReturns;
    std::set<std::string> inspected;
    for (const auto& [id, record] : state.operations_) {
        require(detail::text(id, "ID команды") == id, "Ненормализованный ID команды");
        const auto& command = record.signature;
        const auto kind = command.kind;
        require(command.quote.has_value() == (kind == CommandKind::PlaceOrder) &&
                    command.delivery.has_value() == (kind == CommandKind::ChangeDelivery),
                "Лишний или отсутствующий снимок команды");
        require((kind == CommandKind::OpenReturn || kind == CommandKind::AmendReturn) ||
                    command.returnSelections.empty(),
                "Лишние возвратные позиции команды");
        require(kind == CommandKind::InspectReturn || command.inspectionResults.empty(),
                "Лишние результаты осмотра команды");
        const bool returnCommand = kind >= CommandKind::OpenReturn;
        if (returnCommand) {
            shape(command, 1, 0);
            require(kind == CommandKind::RejectReturn || command.texts.front().empty(),
                    "Лишняя причина возвратной команды");
            if (kind == CommandKind::OpenReturn) {
                const auto& request =
                    detail::get(state.returnRequests_, stringResult(record.result));
                require(openedReturns.insert(request.id()).second &&
                            command.entityId == request.orderId() &&
                            command.actor == request.customerId(),
                        "Команда открытия не совпадает с заявкой");
            } else {
                const auto& request = detail::get(state.returnRequests_, command.entityId);
                if (kind == CommandKind::WithdrawReturn || kind == CommandKind::AmendReturn) {
                    require(command.actor == request.customerId(), "Чужая возвратная команда");
                } else {
                    require(command.actor ==
                                (kind == CommandKind::ExpireReturn ? "system" : "operator"),
                            "Неверная роль возвратной команды");
                }
                switch (kind) {
                case CommandKind::InspectReturn: {
                    const auto& inspection =
                        detail::get(state.inspections_, stringResult(record.result));
                    require(inspected.insert(inspection.id()).second &&
                                inspection.requestId() == request.id(),
                            "Команда осмотра не совпадает с документом");
                    std::vector<InspectionResult> expected;
                    for (const auto& [sku, result] : inspection.results()) {
                        expected.push_back(result);
                    }
                    require(command.inspectionResults == expected,
                            "Журнал осмотра не совпадает с результатами");
                    break;
                }
                case CommandKind::RejectReturn:
                    emptyResult(record.result);
                    require(request.status() == ReturnStatus::Rejected &&
                                request.decisionReason() == command.texts.front(),
                            "Команда отказа не совпадает с решением");
                    break;
                case CommandKind::WithdrawReturn:
                    emptyResult(record.result);
                    require(request.status() == ReturnStatus::Withdrawn,
                            "Команда отзыва не совпадает с состоянием заявки");
                    break;
                case CommandKind::CompleteReturn: {
                    const auto* amount = std::get_if<Money>(&record.result);
                    require(amount && request.status() == ReturnStatus::Completed &&
                                request.inspectionId(),
                            "Неверный результат завершения возврата");
                    const auto order = state.orders_.order(request.orderId());
                    const auto& inspection =
                        detail::get(state.inspections_, *request.inspectionId());
                    require(*amount == requestAmount(request, inspection, *order),
                            "Итог возврата в журнале не совпадает с покупкой");
                    require(matchingEvent(*order, OrderEventKind::ReturnCompleted, id)
                                    .relatedEntityId() == request.id(),
                            "Завершена другая заявка возврата");
                    break;
                }
                case CommandKind::AmendReturn: emptyResult(record.result); break;
                case CommandKind::ExpireReturn:
                    emptyResult(record.result);
                    require(request.status() == ReturnStatus::Rejected &&
                                request.decisionReason() == "NotReceivedInTime",
                            "Истечение возврата не совпадает с решением");
                    break;
                default: throw SnapshotException("Неизвестная возвратная команда");
                }
            }
            if (kind == CommandKind::OpenReturn || kind == CommandKind::AmendReturn) {
                const auto order = state.orders_.order(
                    kind == CommandKind::OpenReturn
                        ? command.entityId
                        : detail::get(state.returnRequests_, command.entityId).orderId());
                require(!command.returnSelections.empty(), "Пустой сохранённый запрос возврата");
                std::set<std::string> skus;
                for (const auto& selection : command.returnSelections) {
                    const auto line = order->line(selection.sku);
                    require(skus.insert(selection.sku).second && selection.quantity > 0 &&
                                selection.quantity <= line.quantity() &&
                                order->returnPolicy().allowedReasons().contains(selection.reason),
                            "Неверный состав возвратной команды");
                }
            }
            continue;
        }
        if (kind == CommandKind::MoveToWishlist) {
            state.customer(command.actor);
            require(command.entityId.empty() && command.numbers.empty() && !command.texts.empty(),
                    "Неверная команда переноса в избранное");
            require(std::is_sorted(command.texts.begin(), command.texts.end()) &&
                        std::adjacent_find(command.texts.begin(), command.texts.end()) ==
                            command.texts.end(),
                    "Повторные или несортированные SKU переноса");
            for (const auto& sku : command.texts) {
                state.catalog_.variant(sku);
            }
            emptyResult(record.result);
            continue;
        }
        const auto order = state.orders_.order(
            kind == CommandKind::PlaceOrder ? stringResult(record.result) : command.entityId);
        const bool operatorCommand =
            kind == CommandKind::StartPicking || kind == CommandKind::Pick ||
            kind == CommandKind::UndoPick || kind == CommandKind::Pack ||
            kind == CommandKind::ReopenPicking || kind == CommandKind::Dispatch;
        require(command.actor == (operatorCommand                    ? "operator"
                                  : kind == CommandKind::ExpireOrder ? "system"
                                                                     : order->customerId()),
                "Неверный инициатор команды заказа");
        switch (kind) {
        case CommandKind::PlaceOrder: {
            shape(command, 0, 0);
            require(command.entityId.empty() && placedOrders.insert(order->id()).second,
                    "Повторное оформление одного заказа");
            command.quote->assertUsable(order->createdAt());
            const auto& initial = std::get<InitialOrderTerms>(order->history().front().payload());
            const PriceQuote terms(
                order->customerId(), order->quoteLines(), initial.delivery.address,
                initial.delivery.optionId, initial.delivery.fee, order->discountSource(),
                command.quote->createdAt(), command.quote->validUntil(), order->returnPolicy());
            require(terms.sameCommercialTerms(*command.quote) &&
                        order->history().front().operationId() == id,
                    "Оформление не совпадает с подтверждённым предложением");
            break;
        }
        case CommandKind::Pay: {
            shape(command, 1, 3);
            require(command.numbers[1] >= 0 && command.numbers[1] <= 1 && command.numbers[2] >= 0 &&
                        command.numbers[2] <= 1 &&
                        std::holds_alternative<AttemptOutcome>(record.result),
                    "Неверные параметры оплаты");
            require(state.payments_.operationIndex().contains(id),
                    "Команда оплаты без финансовой операции");
            break;
        }
        case CommandKind::Cancel:
            shape(command, 0, 0);
            emptyResult(record.result);
            matchingEvent(*order, OrderEventKind::Cancelled, id);
            break;
        case CommandKind::ExpireOrder:
            shape(command, 0, 0);
            emptyResult(record.result);
            matchingEvent(*order, OrderEventKind::Expired, id);
            break;
        case CommandKind::StartPicking:
            shape(command, 0, 0);
            require(
                matchingEvent(*order, OrderEventKind::FulfillmentStarted, id).relatedEntityId() ==
                    stringResult(record.result),
                "Команда создала другое исполнение");
            break;
        case CommandKind::Pick:
        case CommandKind::UndoPick:
            shape(command, 1, 1);
            emptyResult(record.result);
            require(command.numbers[0] > 0 &&
                        command.numbers[0] <= order->line(command.texts[0]).quantity(),
                    "Неверное количество команды подбора");
            break;
        case CommandKind::Pack:
        case CommandKind::ReopenPicking:
            shape(command, 1, 1);
            emptyResult(record.result);
            require(command.texts[0].empty() && command.numbers[0] == 0,
                    "Лишние параметры упаковки");
            break;
        case CommandKind::Dispatch:
            shape(command, 1, 0);
            emptyResult(record.result);
            require(detail::get(state.fulfillments_, order->id()).trackingNumber() ==
                        command.texts[0],
                    "Трек-номер не совпадает с командой");
            matchingEvent(*order, OrderEventKind::Shipped, id);
            break;
        case CommandKind::Deliver:
            shape(command, 0, 0);
            emptyResult(record.result);
            matchingEvent(*order, OrderEventKind::Delivered, id);
            break;
        case CommandKind::ExtendPayment:
            shape(command, 0, 0);
            emptyResult(record.result);
            matchingEvent(*order, OrderEventKind::DeadlineExtended, id);
            break;
        case CommandKind::ChangeDelivery:
            shape(command, 0, 0);
            emptyResult(record.result);
            require(std::get<DeliveryChange>(
                        matchingEvent(*order, OrderEventKind::DeliveryChanged, id).payload())
                            .after == *command.delivery,
                    "Смена доставки не совпадает с журналом");
            break;
        case CommandKind::RepeatOrder:
            require(!command.texts.empty() && command.numbers.empty(),
                    "Пустой состав повторной покупки");
            emptyResult(record.result);
            require(std::is_sorted(command.texts.begin(), command.texts.end()) &&
                        std::adjacent_find(command.texts.begin(), command.texts.end()) ==
                            command.texts.end(),
                    "Неверные SKU повторной покупки");
            for (const auto& sku : command.texts) {
                order->line(sku);
            }
            break;
        default: throw SnapshotException("Неизвестная сохранённая команда");
        }
    }
    require(placedOrders.size() == state.orders_.orders().size() &&
                openedReturns.size() == state.returnRequests_.size() &&
                inspected.size() == state.inspections_.size(),
            "Документы не покрыты журналом команд");
    for (const auto& [id, order] : state.orders_.orders()) {
        for (const auto& event : order->history()) {
            require(state.operations_.contains(event.operationId()), "Событие заказа без команды");
        }
    }
}
} // namespace shop
