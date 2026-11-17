#pragma once

#include "shop/common/Address.hpp"
#include "shop/common/Money.hpp"
#include "shop/common/TimeWindow.hpp"
#include <string>
#include <string_view>
#include <variant>

namespace shop {
enum class OrderEventKind {
    Created,
    PaymentConfirmed,
    Cancelled,
    Expired,
    FulfillmentStarted,
    Shipped,
    Delivered,
    DeadlineExtended,
    DeliveryChanged,
    ReturnCompleted
};
struct DeliveryTerms {
    Address address;
    std::string optionId;
    Money fee;
    bool operator==(const DeliveryTerms&) const = default;
};
struct InitialOrderTerms {
    Timestamp paymentDeadline = 0;
    DeliveryTerms delivery;
    bool operator==(const InitialOrderTerms&) const = default;
};
struct DeadlineChange {
    Timestamp before = 0;
    Timestamp after = 0;
    bool operator==(const DeadlineChange&) const = default;
};
struct DeliveryChange {
    DeliveryTerms before;
    DeliveryTerms after;
    bool operator==(const DeliveryChange&) const = default;
};
using OrderEventPayload =
    std::variant<std::monostate, InitialOrderTerms, DeadlineChange, DeliveryChange>;

/** OrderEvent.
 * Хранит историю переходов и типизированные данные изменений. Описание предназначено для
 * отображения.
 */
class OrderEvent {
  public:
    OrderEvent(Timestamp timestamp, OrderEventKind kind, std::string_view operationId,
               std::string_view relatedEntityId = {}, std::string_view description = {},
               OrderEventPayload payload = {});
    Timestamp timestamp() const noexcept;
    OrderEventKind kind() const noexcept;
    const std::string& operationId() const noexcept;
    const std::string& relatedEntityId() const noexcept;
    const std::string& description() const noexcept;
    const OrderEventPayload& payload() const noexcept;
    bool operator==(const OrderEvent&) const = default;

  private:
    Timestamp timestamp_;
    OrderEventKind kind_;
    std::string operationId_;
    std::string relatedEntityId_;
    std::string description_;
    OrderEventPayload payload_;
};
} // namespace shop
