#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"
#include "shop/common/Validation.hpp"

namespace shop::snapshot {
Json orderValue(const Order& item) {
    return {{"id", item.id()},
            {"customer_id", item.customerId()},
            {"lines",
             arrayOf(item.lines(), [](const auto& line) { return value(line.asQuoteLine()); })},
            {"delivery", value(DeliveryTerms{item.deliveryAddress(), item.deliveryOptionId(),
                                             item.deliveryFee()})},
            {"discount_source", value(item.discountSource())},
            {"created_at", item.createdAt()},
            {"payment_deadline", item.paymentDeadline()},
            {"reservation_id", item.reservationId()},
            {"status", enumValue(item.status())},
            {"history", arrayOf(item.history(), [](const auto& entry) { return value(entry); })},
            {"return_policy", value(item.returnPolicy())},
            {"deadline_extended", item.deadlineExtended()}};
}
Order order(const Json& data) {
    object(data);
    const auto history = vectorOf<OrderEvent>(data.at("history"), event);
    require(!history.empty() && history.front().kind() == OrderEventKind::Created,
            "Нет исходного события заказа");
    const auto createdAt = integer(data.at("created_at"));
    const auto& initial = std::get<InitialOrderTerms>(history.front().payload());
    require(history.front().timestamp() == createdAt,
            "Время создания заказа не совпадает с историей");
    const PriceQuote savedQuote(
        string(data.at("customer_id")), vectorOf<QuoteLine>(data.at("lines"), quoteLine),
        initial.delivery.address, initial.delivery.optionId, initial.delivery.fee,
        discountSource(data.at("discount_source")), createdAt, detail::addNonnegative(createdAt, 1),
        returnsPolicy(data.at("return_policy")));
    Order result(string(data.at("id")), savedQuote, string(data.at("reservation_id")), createdAt,
                 initial.paymentDeadline, history.front().operationId());
    for (std::size_t index = 1; index < history.size(); ++index) {
        const auto& entry = history[index];
        const auto now = entry.timestamp();
        const auto& operation = entry.operationId();
        const auto& related = entry.relatedEntityId();
        switch (entry.kind()) {
        case OrderEventKind::Created:
            throw SnapshotException("Повторное создание заказа в истории");
        case OrderEventKind::PaymentConfirmed:
            result.confirmPayment(now, operation, related);
            break;
        case OrderEventKind::Cancelled: result.cancel(now, operation); break;
        case OrderEventKind::Expired: result.expire(now, operation); break;
        case OrderEventKind::FulfillmentStarted:
            result.beginFulfillment(now, operation, related);
            break;
        case OrderEventKind::Shipped: result.markShipped(now, operation, related); break;
        case OrderEventKind::Delivered: result.markDelivered(now, operation, related); break;
        case OrderEventKind::DeadlineExtended: {
            const auto& change = std::get<DeadlineChange>(entry.payload());
            require(change.before == result.paymentDeadline(),
                    "Старая граница продления не совпадает с историей");
            result.extendPaymentDeadline(now, change.after - change.before, operation);
            break;
        }
        case OrderEventKind::DeliveryChanged: {
            const auto& change = std::get<DeliveryChange>(entry.payload());
            require(change.before == DeliveryTerms{result.deliveryAddress(),
                                                   result.deliveryOptionId(), result.deliveryFee()},
                    "Старые условия доставки не совпадают с историей");
            result.changeUnpaidDelivery(now, change.after, operation);
            break;
        }
        case OrderEventKind::ReturnCompleted:
            result.recordReturnCompleted(now, operation, related);
            break;
        }
    }
    require(result.history() == history, "История заказа содержит противоречивые данные");
    require(enumeration(data.at("status"), OrderStatus::Expired) == result.status() &&
                boolean(data.at("deadline_extended")) == result.deadlineExtended() &&
                integer(data.at("payment_deadline")) == result.paymentDeadline() &&
                deliveryTerms(data.at("delivery")) == DeliveryTerms{result.deliveryAddress(),
                                                                    result.deliveryOptionId(),
                                                                    result.deliveryFee()},
            "Итоговое состояние заказа не совпадает с историей");
    return result;
}
Order paidPrefix(const Order& finalOrder) {
    auto data = orderValue(finalOrder);
    auto& history = data["history"];
    while (!history.empty() &&
           enumeration(history.back().at("kind"), OrderEventKind::ReturnCompleted) !=
               OrderEventKind::PaymentConfirmed) {
        history.erase(history.size() - 1);
    }
    require(!history.empty(), "Нет оплаты для исполнения заказа");
    data["status"] = enumValue(OrderStatus::Confirmed);
    // Delivery/deadline cannot change after successful payment.
    return order(data);
}
} // namespace shop::snapshot

namespace shop {
using namespace snapshot;
OrderBook SnapshotCodec::readOrders(const Json& data) {
    object(data);
    OrderBook result;
    for (const auto& saved : array(data.at("entries"))) {
        result.add(order(saved));
    }
    const auto revision = integer(data.at("revision"));
    require(revision >= result.revision(), "Ревизия заказов меньше числа документов");
    result.revision_ = revision;
    return result;
}
void SnapshotCodec::readFulfillments(StoreState& state, const Json& data) {
    for (const auto& saved : array(data)) {
        const auto order = state.orders_.order(string(saved.at("order_id")));
        Fulfillment item(string(saved.at("id")), paidPrefix(*order),
                         string(saved.at("warehouse_id")));
        require(item.requiredQuantities() == quantities(saved.at("required")),
                "Состав исполнения не совпадает с покупкой");
        const auto picked = quantities(saved.at("picked"));
        require(picked.size() == item.requiredQuantities().size(), "Некорректные позиции подбора");
        const auto status = enumeration(saved.at("status"), FulfillmentStatus::Cancelled);
        if (status != FulfillmentStatus::Created) {
            item.startPicking();
            for (const auto& [sku, quantity] : picked) {
                require(quantity >= 0, "Отрицательный подбор");
                if (quantity) {
                    item.pick(sku, quantity);
                }
            }
            if (status == FulfillmentStatus::Packed || status == FulfillmentStatus::HandedOver ||
                status == FulfillmentStatus::Delivered) {
                item.pack();
            }
            if (status == FulfillmentStatus::HandedOver || status == FulfillmentStatus::Delivered) {
                item.handover(string(saved.at("tracking_number")));
            }
            if (status == FulfillmentStatus::Delivered) {
                item.deliver(integer(saved.at("delivered_at")));
            }
            if (status == FulfillmentStatus::Cancelled) {
                item.cancel();
            }
        }
        require(item.pickedQuantities() == picked &&
                    item.trackingNumber() == optionalString(saved.at("tracking_number")),
                "Несогласованный подбор или трек-номер");
        if (item.deliveredAt()) {
            require(*item.deliveredAt() == integer(saved.at("delivered_at")),
                    "Неверное время получения");
        } else {
            require(saved.at("delivered_at").is_null(), "Время получения без получения");
        }
        insertUnique(state.fulfillments_, item.orderId(), item);
    }
}
} // namespace shop
