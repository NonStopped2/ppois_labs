#include "SnapshotJson.hpp"

namespace shop::snapshot {
Json value(const OrderEvent& item) {
    Json payload = nullptr;
    if (const auto* initial = std::get_if<InitialOrderTerms>(&item.payload())) {
        payload = {{"payment_deadline", initial->paymentDeadline},
                   {"delivery", value(initial->delivery)}};
    }
    if (const auto* change = std::get_if<DeadlineChange>(&item.payload())) {
        payload = {{"before", change->before}, {"after", change->after}};
    }
    if (const auto* change = std::get_if<DeliveryChange>(&item.payload())) {
        payload = {{"before", value(change->before)}, {"after", value(change->after)}};
    }
    return {{"timestamp", item.timestamp()},      {"kind", enumValue(item.kind())},
            {"operation_id", item.operationId()}, {"related_entity_id", item.relatedEntityId()},
            {"description", item.description()},  {"payload", payload}};
}
OrderEvent event(const Json& data) {
    object(data);
    const auto kind = enumeration(data.at("kind"), OrderEventKind::ReturnCompleted);
    const auto& saved = data.at("payload");
    OrderEventPayload payload;
    if (kind == OrderEventKind::Created) {
        object(saved);
        payload = InitialOrderTerms{integer(saved.at("payment_deadline")),
                                    deliveryTerms(saved.at("delivery"))};
    } else if (kind == OrderEventKind::DeadlineExtended) {
        object(saved);
        payload = DeadlineChange{integer(saved.at("before")), integer(saved.at("after"))};
    } else if (kind == OrderEventKind::DeliveryChanged) {
        object(saved);
        payload =
            DeliveryChange{deliveryTerms(saved.at("before")), deliveryTerms(saved.at("after"))};
    } else {
        require(saved.is_null(), "Лишние данные события заказа");
    }
    return OrderEvent(integer(data.at("timestamp")), kind, string(data.at("operation_id")),
                      string(data.at("related_entity_id")), string(data.at("description")),
                      std::move(payload));
}
} // namespace shop::snapshot
