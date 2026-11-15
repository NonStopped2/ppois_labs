#include "shop/orders/Order.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/OrderStateException.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>
#include <iterator>
#include <numeric>
#include <type_traits>

namespace shop {
Order::Order(std::string_view id, const PriceQuote& quote, std::string_view reservationId,
             Timestamp createdAt, Timestamp paymentDeadline, std::string_view operationId)
    : id_(detail::text(id, "заказ")), customerId_(quote.customerId()),
      deliveryAddress_(quote.deliveryAddress()), deliveryOptionId_(quote.deliveryOptionId()),
      deliveryFee_(quote.deliveryFee()), discountSource_(quote.discountSource()),
      createdAt_(createdAt), paymentDeadline_(paymentDeadline),
      reservationId_(detail::text(reservationId, "резерв заказа")),
      returnPolicySnapshot_(quote.returnPolicy()) {
    quote.assertUsable(createdAt);
    detail::require(paymentDeadline > createdAt, "Неверный срок оплаты");
    std::transform(quote.lines().begin(), quote.lines().end(), std::back_inserter(lines_),
                   [](const auto& entry) { return OrderLine(entry); });
    history_.emplace_back(
        createdAt, OrderEventKind::Created, operationId, reservationId, "Заказ оформлен",
        InitialOrderTerms{paymentDeadline, {deliveryAddress_, deliveryOptionId_, deliveryFee_}});
}
const std::string& Order::id() const noexcept {
    return id_;
}
const std::string& Order::customerId() const noexcept {
    return customerId_;
}
const std::vector<OrderLine>& Order::lines() const noexcept {
    return lines_;
}
const Address& Order::deliveryAddress() const noexcept {
    return deliveryAddress_;
}
const std::string& Order::deliveryOptionId() const noexcept {
    return deliveryOptionId_;
}
Money Order::deliveryFee() const noexcept {
    return deliveryFee_;
}
const DiscountSource& Order::discountSource() const noexcept {
    return discountSource_;
}
Timestamp Order::createdAt() const noexcept {
    return createdAt_;
}
Timestamp Order::paymentDeadline() const noexcept {
    return paymentDeadline_;
}
const std::string& Order::reservationId() const noexcept {
    return reservationId_;
}
OrderStatus Order::status() const noexcept {
    return status_;
}
const std::vector<OrderEvent>& Order::history() const noexcept {
    return history_;
}
const ReturnPolicy& Order::returnPolicy() const noexcept {
    return returnPolicySnapshot_;
}
bool Order::deadlineExtended() const noexcept {
    return deadlineExtended_;
}
Money Order::goodsNet() const {
    return std::accumulate(lines_.begin(), lines_.end(), Money{}, [](Money sum, const auto& entry) {
        return sum.checkedAdd(entry.netTotal());
    });
}
Money Order::total() const {
    return goodsNet().checkedAdd(deliveryFee_);
}
OrderLine Order::line(std::string_view sku) const {
    const auto found = std::find_if(lines_.begin(), lines_.end(),
                                    [sku](const auto& entry) { return entry.sku() == sku; });
    detail::check<EntityNotFoundException>(found != lines_.end(), "Позиция отсутствует в заказе");
    return *found;
}
std::map<std::string, std::int64_t> Order::quantities() const {
    std::map<std::string, std::int64_t> result;
    std::transform(lines_.begin(), lines_.end(), std::inserter(result, result.end()),
                   [](const auto& entry) { return std::make_pair(entry.sku(), entry.quantity()); });
    return result;
}
std::vector<QuoteLine> Order::quoteLines() const {
    std::vector<QuoteLine> result;
    std::transform(lines_.begin(), lines_.end(), std::back_inserter(result),
                   [](const auto& entry) { return entry.asQuoteLine(); });
    return result;
}
std::optional<Timestamp> Order::deliveredAt() const noexcept {
    const auto found = std::find_if(history_.begin(), history_.end(), [](const auto& event) {
        return event.kind() == OrderEventKind::Delivered;
    });
    return found == history_.end() ? std::nullopt : std::optional<Timestamp>(found->timestamp());
}
HistoricalOrderState Order::stateBefore(Timestamp end) const {
    detail::require(end > createdAt_, "Заказ ещё не существовал на границе отчёта");
    const auto& initial = std::get<InitialOrderTerms>(history_.front().payload());
    HistoricalOrderState result{OrderStatus::AwaitingPayment, initial.paymentDeadline,
                                initial.delivery};
    for (const auto& event : history_) {
        if (event.timestamp() >= end) {
            break;
        }
        switch (event.kind()) {
        case OrderEventKind::Created: break;
        case OrderEventKind::PaymentConfirmed: result.status = OrderStatus::Confirmed; break;
        case OrderEventKind::Cancelled: result.status = OrderStatus::Cancelled; break;
        case OrderEventKind::Expired: result.status = OrderStatus::Expired; break;
        case OrderEventKind::FulfillmentStarted: result.status = OrderStatus::InFulfillment; break;
        case OrderEventKind::Shipped: result.status = OrderStatus::Shipped; break;
        case OrderEventKind::Delivered: result.status = OrderStatus::Delivered; break;
        case OrderEventKind::DeadlineExtended:
            result.paymentDeadline = std::get<DeadlineChange>(event.payload()).after;
            break;
        case OrderEventKind::DeliveryChanged:
            result.delivery = std::get<DeliveryChange>(event.payload()).after;
            break;
        case OrderEventKind::ReturnCompleted: break;
        }
    }
    return result;
}
void Order::assertOwner(std::string_view actor) const {
    detail::check<OrderStateException>(actor == customerId_,
                                       "Заказ принадлежит другому покупателю");
}
void Order::checkTime(Timestamp now) const {
    detail::check<OrderStateException>(now >= history_.back().timestamp(),
                                       "Нарушена хронология заказа");
}
void Order::assertAwaitingPayment(Timestamp now) const {
    checkTime(now);
    detail::check<OrderStateException>(status_ == OrderStatus::AwaitingPayment &&
                                           now < paymentDeadline_,
                                       "Заказ не ожидает оплату или истёк");
}
void Order::transition(OrderStatus expected, OrderStatus target, const OrderEvent& event) {
    checkTime(event.timestamp());
    detail::check<OrderStateException>(status_ == expected, "Недопустимое состояние заказа");
    history_.push_back(event);
    status_ = target;
}
void Order::confirmPayment(Timestamp now, std::string_view operationId,
                           std::string_view paymentId) {
    assertAwaitingPayment(now);
    transition(
        OrderStatus::AwaitingPayment, OrderStatus::Confirmed,
        {now, OrderEventKind::PaymentConfirmed, operationId, detail::text(paymentId, "платёж")});
}
void Order::cancel(Timestamp now, std::string_view operationId) {
    checkTime(now);
    detail::check<OrderStateException>(status_ == OrderStatus::AwaitingPayment ||
                                           status_ == OrderStatus::Confirmed ||
                                           status_ == OrderStatus::InFulfillment,
                                       "Отменить можно только неотправленный заказ");
    history_.emplace_back(now, OrderEventKind::Cancelled, operationId);
    status_ = OrderStatus::Cancelled;
}
void Order::expire(Timestamp now, std::string_view operationId) {
    detail::check<OrderStateException>(now >= paymentDeadline_, "Срок оплаты ещё не истёк");
    transition(OrderStatus::AwaitingPayment, OrderStatus::Expired,
               {now, OrderEventKind::Expired, operationId});
}
void Order::beginFulfillment(Timestamp now, std::string_view operationId,
                             std::string_view fulfillmentId) {
    transition(OrderStatus::Confirmed, OrderStatus::InFulfillment,
               {now, OrderEventKind::FulfillmentStarted, operationId,
                detail::text(fulfillmentId, "исполнение")});
}
void Order::markShipped(Timestamp now, std::string_view operationId,
                        std::string_view fulfillmentId) {
    transition(
        OrderStatus::InFulfillment, OrderStatus::Shipped,
        {now, OrderEventKind::Shipped, operationId, detail::text(fulfillmentId, "исполнение")});
}
void Order::markDelivered(Timestamp now, std::string_view operationId,
                          std::string_view fulfillmentId) {
    transition(
        OrderStatus::Shipped, OrderStatus::Delivered,
        {now, OrderEventKind::Delivered, operationId, detail::text(fulfillmentId, "исполнение")});
}
void Order::extendPaymentDeadline(Timestamp now, std::int64_t extension,
                                  std::string_view operationId) {
    assertAwaitingPayment(now);
    detail::check<OrderStateException>(!deadlineExtended_ && extension > 0,
                                       "Продление уже использовано или неположительно");
    const auto nextDeadline = detail::addNonnegative(paymentDeadline_, extension);
    history_.emplace_back(now, OrderEventKind::DeadlineExtended, operationId, "", "",
                          DeadlineChange{paymentDeadline_, nextDeadline});
    paymentDeadline_ = nextDeadline;
    deadlineExtended_ = true;
}
void Order::changeUnpaidDelivery(Timestamp now, DeliveryTerms terms, std::string_view operationId) {
    assertAwaitingPayment(now);
    terms.optionId = detail::text(terms.optionId, "доставка");
    detail::require(terms.fee >= Money{} && goodsNet().checkedAdd(terms.fee) > Money{},
                    "Недопустимый новый итог заказа");
    const DeliveryChange change{{deliveryAddress_, deliveryOptionId_, deliveryFee_}, terms};
    static_assert(std::is_nothrow_swappable_v<Address>);
    history_.emplace_back(now, OrderEventKind::DeliveryChanged, operationId, "", "", change);
    using std::swap;
    swap(deliveryAddress_, terms.address);
    deliveryOptionId_.swap(terms.optionId);
    deliveryFee_ = terms.fee;
}
void Order::recordReturnCompleted(Timestamp now, std::string_view operationId,
                                  std::string_view requestId) {
    checkTime(now);
    detail::check<OrderStateException>(status_ == OrderStatus::Delivered,
                                       "Возврат требует полученного заказа");
    const auto key = detail::text(requestId, "заявка возврата");
    detail::check<OrderStateException>(
        std::none_of(history_.begin(), history_.end(),
                     [&key](const auto& event) {
                         return event.kind() == OrderEventKind::ReturnCompleted &&
                                event.relatedEntityId() == key;
                     }),
        "Возврат уже завершён");
    history_.emplace_back(now, OrderEventKind::ReturnCompleted, operationId, key);
}
} // namespace shop
