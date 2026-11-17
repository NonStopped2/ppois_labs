#pragma once

#include "shop/orders/Order.hpp"
#include "shop/returns/ReturnLine.hpp"
#include <optional>
#include <vector>

namespace shop {
class ReturnInspection;
enum class ReturnStatus { Submitted, Inspected, Completed, Rejected, Withdrawn };
/** ReturnRequest.
 * Хранит заявку и её исход. Связывает запрос с осмотром и денежным возвратом после
 * обработки.
 */
class ReturnRequest {
  public:
    ReturnRequest(std::string_view id, const Order& order, std::vector<ReturnLine> lines,
                  Timestamp createdAt);
    const std::string& id() const noexcept;
    const std::string& orderId() const noexcept;
    const std::string& customerId() const noexcept;
    const std::vector<ReturnLine>& lines() const noexcept;
    Timestamp createdAt() const noexcept;
    Timestamp deadline() const noexcept;
    ReturnStatus status() const noexcept;
    const std::optional<std::string>& inspectionId() const noexcept;
    const std::optional<std::string>& refundId() const noexcept;
    const std::string& decisionReason() const noexcept;
    bool active() const noexcept;
    Money maximumRefund() const;
    ReturnLine line(std::string_view sku) const;
    void assertOwner(std::string_view actor) const;
    void attachInspection(const ReturnInspection& inspection);
    void amend(std::vector<ReturnLine> lines, const Order& order, Timestamp now);
    void withdraw(std::string_view actor, Timestamp now);
    void reject(std::string_view reason, Timestamp now);
    void complete(std::optional<std::string> refundId);
    bool operator==(const ReturnRequest&) const = default;

  private:
    static void validateLines(const std::vector<ReturnLine>& lines, const Order& order,
                              Timestamp now);
    std::string id_;
    std::string orderId_;
    std::string customerId_;
    std::vector<ReturnLine> lines_;
    Timestamp createdAt_;
    Timestamp deadline_;
    ReturnStatus status_ = ReturnStatus::Submitted;
    std::optional<std::string> inspectionId_;
    std::optional<std::string> refundId_;
    std::string decisionReason_;
};
} // namespace shop
