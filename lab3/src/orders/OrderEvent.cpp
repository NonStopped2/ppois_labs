#include "shop/orders/OrderEvent.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
OrderEvent::OrderEvent(Timestamp timestamp, OrderEventKind kind, std::string_view operationId,
                       std::string_view relatedEntityId, std::string_view description,
                       OrderEventPayload payload)
    : timestamp_(timestamp), kind_(kind),
      operationId_(detail::text(operationId, "операция истории")),
      relatedEntityId_(detail::text(relatedEntityId, "связанный документ", true)),
      description_(detail::text(description, "описание события", true)),
      payload_(std::move(payload)) {
    detail::require(timestamp >= 0 && kind >= OrderEventKind::Created &&
                        kind <= OrderEventKind::ReturnCompleted,
                    "Некорректное событие заказа");
    const auto expected = kind == OrderEventKind::Created            ? 1U
                          : kind == OrderEventKind::DeadlineExtended ? 2U
                          : kind == OrderEventKind::DeliveryChanged  ? 3U
                                                                     : 0U;
    detail::require(payload_.index() == expected, "Тип данных не соответствует событию");
    if (const auto* initial = std::get_if<InitialOrderTerms>(&payload_)) {
        detail::require(initial->paymentDeadline > timestamp && initial->delivery.fee >= Money{} &&
                            !initial->delivery.optionId.empty(),
                        "Некорректные исходные условия заказа");
    }
    if (const auto* change = std::get_if<DeadlineChange>(&payload_)) {
        detail::require(change->before > timestamp && change->after > change->before,
                        "Некорректное продление срока");
    }
    if (const auto* change = std::get_if<DeliveryChange>(&payload_)) {
        detail::require(change->before.fee >= Money{} && change->after.fee >= Money{} &&
                            !change->before.optionId.empty() && !change->after.optionId.empty(),
                        "Некорректная смена доставки");
    }
}
Timestamp OrderEvent::timestamp() const noexcept {
    return timestamp_;
}
OrderEventKind OrderEvent::kind() const noexcept {
    return kind_;
}
const std::string& OrderEvent::operationId() const noexcept {
    return operationId_;
}
const std::string& OrderEvent::relatedEntityId() const noexcept {
    return relatedEntityId_;
}
const std::string& OrderEvent::description() const noexcept {
    return description_;
}
const OrderEventPayload& OrderEvent::payload() const noexcept {
    return payload_;
}
} // namespace shop
