#include "shop/application/OrderProcessing.hpp"
#include "shop/application/PreparedChange.hpp"
#include "shop/common/CartRuleException.hpp"
#include "shop/common/DeliveryRuleException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/OrderStateException.hpp"
#include "shop/common/PaymentRuleException.hpp"
#include "shop/common/QuoteChangedException.hpp"
#include "shop/common/ReservationStateException.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {
namespace {
CommandSignature command(CommandKind kind, std::string_view actor, std::string_view entity,
                         std::vector<std::string> texts = {},
                         std::vector<std::int64_t> numbers = {},
                         std::optional<DeliveryTerms> delivery = std::nullopt) {
    return {kind,
            std::string(actor),
            std::string(entity),
            std::move(texts),
            std::move(numbers),
            std::move(delivery),
            std::nullopt};
}
void assertReservation(const Inventory& inventory, const Order& order, ReservationStatus expected) {
    const auto reservation = inventory.reservation(order.reservationId());
    detail::check<ReservationStateException>(
        reservation.orderId() == order.id() && reservation.quantities() == order.quantities() &&
            reservation.status() == expected &&
            reservation.warehouseId() == inventory.warehouse().id() &&
            reservation.expiresAt() == order.paymentDeadline(),
        "Резерв не согласован с заказом");
}
} // namespace
OrderProcessing::OrderProcessing(StoreState& state) : state_(state) {}
AttemptOutcome OrderProcessing::pay(std::string_view actor, std::string_view orderId, Money amount,
                                    PaymentMethod method, bool accepted,
                                    std::string_view failureReason, std::string_view operationId) {
    const auto reason = detail::text(failureReason, "причина отказа", accepted);
    const auto signature = command(CommandKind::Pay, actor, orderId, {reason},
                                   {amount.kopecks(), static_cast<std::int64_t>(method), accepted});
    if (const auto replay = replayOperation(state_.operations_, operationId, signature)) {
        return std::get<AttemptOutcome>(*replay);
    }
    const auto now = state_.now();
    auto order = *state_.orders_.order(orderId);
    order.assertOwner(actor);
    order.assertAwaitingPayment(now);
    assertReservation(state_.inventory_, order, ReservationStatus::Held);
    const auto attemptId = state_.nextId("attempt");
    const auto paymentId = accepted ? state_.nextId("payment") : std::string{};
    PreparedChange change(state_);
    auto result = change.payments().recordAttempt(attemptId, paymentId, order, amount, method, now,
                                                  accepted, reason, operationId);
    if (accepted) {
        change.inventory().confirm(order.reservationId(), now);
        if (order.discountSource().couponCode) {
            change.coupons().confirmUse(order.id(), now);
        }
        order.confirmPayment(now, operationId, *result.paymentId);
        change.orders().replace(order);
    }
    recordOperation(change.operations(), operationId, signature, result);
    change.commit();
    return result;
}
void OrderProcessing::cancel(std::string_view actor, std::string_view orderId,
                             std::string_view operationId) {
    const auto signature = command(CommandKind::Cancel, actor, orderId);
    if (replayOperation(state_.operations_, operationId, signature)) {
        return;
    }
    const auto now = state_.now();
    auto order = *state_.orders_.order(orderId);
    order.assertOwner(actor);
    const bool unpaid = order.status() == OrderStatus::AwaitingPayment;
    order.cancel(now, operationId);
    assertReservation(state_.inventory_, order,
                      unpaid ? ReservationStatus::Held : ReservationStatus::Confirmed);
    PreparedChange change(state_);
    change.inventory().release(order.reservationId(), now);
    if (unpaid) {
        if (order.discountSource().couponCode) {
            change.coupons().releaseUse(order.id());
        }
    } else {
        const auto payment = state_.payments_.findPaymentForOrder(order.id());
        detail::check<PaymentRuleException>(payment.has_value(),
                                            "У оплаченного заказа отсутствует платёж");
        change.payments().refund(state_.nextId("refund"), operationId, payment->id(), std::nullopt,
                                 order.total(), now);
    }
    const auto shipment = state_.fulfillments_.find(order.id());
    if (shipment != state_.fulfillments_.end()) {
        change.fulfillments().at(order.id()).cancel();
    }
    change.orders().replace(order);
    recordOperation(change.operations(), operationId, signature);
    change.commit();
}
std::string OrderProcessing::startPicking(std::string_view orderId, std::string_view operationId) {
    const auto signature = command(CommandKind::StartPicking, "operator", orderId);
    if (const auto replay = replayOperation(state_.operations_, operationId, signature)) {
        return std::get<std::string>(*replay);
    }
    auto order = *state_.orders_.order(orderId);
    assertReservation(state_.inventory_, order, ReservationStatus::Confirmed);
    auto id = state_.nextId("fulfillment");
    Fulfillment shipment(id, order, state_.inventory_.warehouse().id());
    shipment.startPicking();
    order.beginFulfillment(state_.now(), operationId, id);
    PreparedChange change(state_);
    detail::check<DuplicateEntityException>(
        change.fulfillments().emplace(order.id(), shipment).second, "У заказа уже есть исполнение");
    change.orders().replace(order);
    recordOperation(change.operations(), operationId, signature, id);
    change.commit();
    return id;
}
void OrderProcessing::changeFulfillment(std::string_view orderId, CommandKind kind,
                                        std::string_view sku, std::int64_t quantity,
                                        std::string_view operationId) {
    const auto signature = command(kind, "operator", orderId, {std::string(sku)}, {quantity});
    if (replayOperation(state_.operations_, operationId, signature)) {
        return;
    }
    const auto order = state_.orders_.order(orderId);
    detail::check<OrderStateException>(order->status() == OrderStatus::InFulfillment,
                                       "Заказ не комплектуется");
    auto shipment = detail::get(state_.fulfillments_, orderId);
    switch (kind) {
    case CommandKind::Pick: shipment.pick(sku, quantity); break;
    case CommandKind::UndoPick: shipment.undoPick(sku, quantity); break;
    case CommandKind::Pack: shipment.pack(); break;
    case CommandKind::ReopenPicking: shipment.reopenPicking(); break;
    default: throw InvalidDomainDataException("Неизвестная команда комплектации");
    }
    PreparedChange change(state_);
    change.fulfillments().at(std::string(orderId)) = std::move(shipment);
    recordOperation(change.operations(), operationId, signature);
    change.commit();
}
void OrderProcessing::pick(std::string_view orderId, std::string_view sku, std::int64_t quantity,
                           std::string_view operationId) {
    changeFulfillment(orderId, CommandKind::Pick, sku, quantity, operationId);
}
void OrderProcessing::undoPick(std::string_view orderId, std::string_view sku,
                               std::int64_t quantity, std::string_view operationId) {
    changeFulfillment(orderId, CommandKind::UndoPick, sku, quantity, operationId);
}
void OrderProcessing::pack(std::string_view orderId, std::string_view operationId) {
    changeFulfillment(orderId, CommandKind::Pack, "", 0, operationId);
}
void OrderProcessing::reopenPicking(std::string_view orderId, std::string_view operationId) {
    changeFulfillment(orderId, CommandKind::ReopenPicking, "", 0, operationId);
}
void OrderProcessing::dispatch(std::string_view orderId, std::string_view trackingNumber,
                               std::string_view operationId) {
    const auto tracking = detail::text(trackingNumber, "трек-номер");
    const auto signature = command(CommandKind::Dispatch, "operator", orderId, {tracking});
    if (replayOperation(state_.operations_, operationId, signature)) {
        return;
    }
    const auto now = state_.now();
    auto order = *state_.orders_.order(orderId);
    auto shipment = detail::get(state_.fulfillments_, orderId);
    for (const auto& [id, entry] : state_.fulfillments_) {
        detail::check<DuplicateEntityException>(entry.trackingNumber() != tracking,
                                                "Трек-номер уже назначен");
    }
    assertReservation(state_.inventory_, order, ReservationStatus::Confirmed);
    shipment.handover(tracking);
    order.markShipped(now, operationId, shipment.id());
    PreparedChange change(state_);
    change.inventory().consume(order.reservationId(), now);
    change.fulfillments().at(order.id()) = std::move(shipment);
    change.orders().replace(order);
    recordOperation(change.operations(), operationId, signature);
    change.commit();
}
void OrderProcessing::confirmDelivery(std::string_view actor, std::string_view orderId,
                                      std::string_view operationId) {
    const auto signature = command(CommandKind::Deliver, actor, orderId);
    if (replayOperation(state_.operations_, operationId, signature)) {
        return;
    }
    const auto now = state_.now();
    auto order = *state_.orders_.order(orderId);
    order.assertOwner(actor);
    auto shipment = detail::get(state_.fulfillments_, orderId);
    assertReservation(state_.inventory_, order, ReservationStatus::Consumed);
    order.markDelivered(now, operationId, shipment.id());
    shipment.deliver(now);
    PreparedChange change(state_);
    change.loyalty()
        .at(order.customerId())
        .recordDelivery(order.id(), order.goodsNet(), operationId);
    change.fulfillments().at(order.id()) = std::move(shipment);
    change.orders().replace(order);
    recordOperation(change.operations(), operationId, signature);
    change.commit();
}
void OrderProcessing::extendPaymentDeadline(std::string_view actor, std::string_view orderId,
                                            std::string_view operationId) {
    const auto signature = command(CommandKind::ExtendPayment, actor, orderId);
    if (replayOperation(state_.operations_, operationId, signature)) {
        return;
    }
    const auto now = state_.now();
    auto order = *state_.orders_.order(orderId);
    order.assertOwner(actor);
    assertReservation(state_.inventory_, order, ReservationStatus::Held);
    order.extendPaymentDeadline(now, state_.paymentExtension_, operationId);
    PreparedChange change(state_);
    change.inventory().extend(order.reservationId(), now, order.paymentDeadline());
    change.orders().replace(order);
    recordOperation(change.operations(), operationId, signature);
    change.commit();
}
DeliveryTerms OrderProcessing::previewUnpaidDelivery(std::string_view actor,
                                                     std::string_view orderId,
                                                     const Address& destination,
                                                     std::string_view deliveryOptionId) const {
    const auto order = state_.orders_.order(orderId);
    order->assertOwner(actor);
    order->assertAwaitingPayment(state_.now());
    const auto option = state_.deliveryPolicy_.option(deliveryOptionId);
    detail::check<DeliveryRuleException>(option.accepts(destination, order->quoteLines()),
                                         "Доставка не подходит сохранённой покупке");
    const auto fee = option.calculateFee(order->goodsNet());
    detail::check<DeliveryRuleException>(order->goodsNet().checkedAdd(fee) > Money{},
                                         "Смена доставки создаёт нулевой заказ");
    return {destination, option.id(), fee};
}
void OrderProcessing::changeUnpaidDelivery(std::string_view actor, std::string_view orderId,
                                           const DeliveryTerms& acceptedTerms,
                                           std::string_view operationId) {
    const auto signature =
        command(CommandKind::ChangeDelivery, actor, orderId, {}, {}, acceptedTerms);
    if (replayOperation(state_.operations_, operationId, signature)) {
        return;
    }
    const auto current =
        previewUnpaidDelivery(actor, orderId, acceptedTerms.address, acceptedTerms.optionId);
    detail::check<QuoteChangedException>(current == acceptedTerms,
                                         "Подтвердите актуальную стоимость доставки");
    auto order = *state_.orders_.order(orderId);
    order.changeUnpaidDelivery(state_.now(), current, operationId);
    PreparedChange change(state_);
    change.orders().replace(order);
    recordOperation(change.operations(), operationId, signature);
    change.commit();
}
std::vector<RepeatItem> OrderProcessing::repeatOrderPlan(std::string_view actor,
                                                         std::string_view orderId) const {
    const auto order = state_.orders_.order(orderId);
    order->assertOwner(actor);
    std::vector<RepeatItem> result;
    for (const auto& entry : order->lines()) {
        const bool available =
            state_.catalog_.productForSku(entry.sku()).status() == ProductStatus::Published &&
            state_.catalog_.variant(entry.sku()).saleEnabled() &&
            state_.inventory_.available(entry.sku()) >= entry.quantity();
        result.push_back({entry.sku(), entry.quantity(), available});
    }
    return result;
}
void OrderProcessing::repeatOrder(std::string_view actor, std::string_view orderId,
                                  const std::vector<std::string>& selectedSkus,
                                  std::string_view operationId) {
    auto selected = selectedSkus;
    std::sort(selected.begin(), selected.end());
    const auto signature = command(CommandKind::RepeatOrder, actor, orderId, selected);
    if (replayOperation(state_.operations_, operationId, signature)) {
        return;
    }
    const auto plan = repeatOrderPlan(actor, orderId);
    detail::check<CartRuleException>(!selected.empty(), "Не выбраны позиции повторной покупки");
    detail::check<DuplicateEntityException>(std::adjacent_find(selected.begin(), selected.end()) ==
                                                selected.end(),
                                            "Повторный SKU в повторной покупке");
    auto cart = state_.cart(actor);
    for (const auto& sku : selected) {
        const auto found = std::find_if(plan.begin(), plan.end(),
                                        [&sku](const auto& entry) { return entry.sku == sku; });
        detail::check<CartRuleException>(found != plan.end() && found->available,
                                         "Позиция недоступна для повторной покупки");
        cart.addItem(state_.catalog_, sku, found->quantity, state_.now(),
                     state_.pricingPolicy_.maximumCartUnits());
    }
    PreparedChange change(state_);
    change.carts().at(std::string(actor)) = std::move(cart);
    recordOperation(change.operations(), operationId, signature);
    change.commit();
}
} // namespace shop
