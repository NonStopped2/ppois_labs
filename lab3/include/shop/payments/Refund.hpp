#pragma once

#include "shop/common/Money.hpp"
#include "shop/common/TimeWindow.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace shop {
/** Refund.
 * Фиксирует положительный денежный возврат по платежу. При возврате товара указывает заявку.
 * Возврат товара на нулевую сумму отражается в истории заказа.
 */
class Refund {
  public:
    Refund(std::string_view id, std::string_view operationId, std::string_view paymentId,
           std::string_view orderId, std::optional<std::string> returnRequestId, Money amount,
           Timestamp createdAt);
    const std::string& id() const noexcept;
    const std::string& operationId() const noexcept;
    const std::string& paymentId() const noexcept;
    const std::string& orderId() const noexcept;
    const std::optional<std::string>& returnRequestId() const noexcept;
    Money amount() const noexcept;
    Timestamp createdAt() const noexcept;
    bool operator==(const Refund&) const = default;

  private:
    std::string id_;
    std::string operationId_;
    std::string paymentId_;
    std::string orderId_;
    std::optional<std::string> returnRequestId_;
    Money amount_;
    Timestamp createdAt_;
};
} // namespace shop
