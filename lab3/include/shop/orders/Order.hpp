#pragma once

#include "shop/orders/OrderEvent.hpp"
#include "shop/orders/OrderLine.hpp"
#include "shop/pricing/PriceQuote.hpp"
#include <map>
#include <optional>
#include <vector>

namespace shop {
enum class OrderStatus {
    AwaitingPayment,
    Confirmed,
    InFulfillment,
    Shipped,
    Delivered,
    Cancelled,
    Expired
};
struct HistoricalOrderState {
    OrderStatus status = OrderStatus::AwaitingPayment;
    Timestamp paymentDeadline = 0;
    DeliveryTerms delivery;
    bool operator==(const HistoricalOrderState&) const = default;
};

/** Order.
 * Управляет фазами покупки и хранит её коммерческий снимок. История позволяет восстановить
 * состояние на момент времени.
 */
class Order {
  public:
    Order(std::string_view id, const PriceQuote& quote, std::string_view reservationId,
          Timestamp createdAt, Timestamp paymentDeadline, std::string_view operationId);
    const std::string& id() const noexcept;
    const std::string& customerId() const noexcept;
    const std::vector<OrderLine>& lines() const noexcept;
    const Address& deliveryAddress() const noexcept;
    const std::string& deliveryOptionId() const noexcept;
    Money deliveryFee() const noexcept;
    const DiscountSource& discountSource() const noexcept;
    Timestamp createdAt() const noexcept;
    Timestamp paymentDeadline() const noexcept;
    const std::string& reservationId() const noexcept;
    OrderStatus status() const noexcept;
    const std::vector<OrderEvent>& history() const noexcept;
    const ReturnPolicy& returnPolicy() const noexcept;
    bool deadlineExtended() const noexcept;
    Money goodsNet() const;
    Money total() const;
    OrderLine line(std::string_view sku) const;
    std::map<std::string, std::int64_t> quantities() const;
    std::vector<QuoteLine> quoteLines() const;
    std::optional<Timestamp> deliveredAt() const noexcept;
    HistoricalOrderState stateBefore(Timestamp end) const;
    void assertOwner(std::string_view actor) const;
    void assertAwaitingPayment(Timestamp now) const;
    void confirmPayment(Timestamp now, std::string_view operationId, std::string_view paymentId);
    void cancel(Timestamp now, std::string_view operationId);
    void expire(Timestamp now, std::string_view operationId);
    void beginFulfillment(Timestamp now, std::string_view operationId,
                          std::string_view fulfillmentId);
    void markShipped(Timestamp now, std::string_view operationId, std::string_view fulfillmentId);
    void markDelivered(Timestamp now, std::string_view operationId, std::string_view fulfillmentId);
    void extendPaymentDeadline(Timestamp now, std::int64_t extension, std::string_view operationId);
    void changeUnpaidDelivery(Timestamp now, DeliveryTerms terms, std::string_view operationId);
    void recordReturnCompleted(Timestamp now, std::string_view operationId,
                               std::string_view requestId);
    bool operator==(const Order&) const = default;

  private:
    void checkTime(Timestamp now) const;
    void transition(OrderStatus expected, OrderStatus target, const OrderEvent& event);
    std::string id_;
    std::string customerId_;
    std::vector<OrderLine> lines_;
    Address deliveryAddress_;
    std::string deliveryOptionId_;
    Money deliveryFee_;
    DiscountSource discountSource_;
    Timestamp createdAt_;
    Timestamp paymentDeadline_;
    std::string reservationId_;
    OrderStatus status_ = OrderStatus::AwaitingPayment;
    std::vector<OrderEvent> history_;
    ReturnPolicy returnPolicySnapshot_;
    bool deadlineExtended_ = false;
};
} // namespace shop
