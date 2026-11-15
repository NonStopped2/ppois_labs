#pragma once

#include "shop/application/StoreState.hpp"

namespace shop {
struct RepeatItem {
    std::string sku;
    std::int64_t quantity = 0;
    bool available = false;
};
class OrderProcessing {
  public:
    explicit OrderProcessing(StoreState& state);
    AttemptOutcome pay(std::string_view actor, std::string_view orderId, Money amount,
                       PaymentMethod method, bool accepted, std::string_view failureReason,
                       std::string_view operationId);
    void cancel(std::string_view actor, std::string_view orderId, std::string_view operationId);
    std::string startPicking(std::string_view orderId, std::string_view operationId);
    void pick(std::string_view orderId, std::string_view sku, std::int64_t quantity,
              std::string_view operationId);
    void undoPick(std::string_view orderId, std::string_view sku, std::int64_t quantity,
                  std::string_view operationId);
    void pack(std::string_view orderId, std::string_view operationId);
    void reopenPicking(std::string_view orderId, std::string_view operationId);
    void dispatch(std::string_view orderId, std::string_view trackingNumber,
                  std::string_view operationId);
    void confirmDelivery(std::string_view actor, std::string_view orderId,
                         std::string_view operationId);
    void extendPaymentDeadline(std::string_view actor, std::string_view orderId,
                               std::string_view operationId);
    DeliveryTerms previewUnpaidDelivery(std::string_view actor, std::string_view orderId,
                                        const Address& destination,
                                        std::string_view deliveryOptionId) const;
    void changeUnpaidDelivery(std::string_view actor, std::string_view orderId,
                              const DeliveryTerms& acceptedTerms, std::string_view operationId);
    std::vector<RepeatItem> repeatOrderPlan(std::string_view actor, std::string_view orderId) const;
    void repeatOrder(std::string_view actor, std::string_view orderId,
                     const std::vector<std::string>& selectedSkus, std::string_view operationId);

  private:
    void changeFulfillment(std::string_view orderId, CommandKind kind, std::string_view sku,
                           std::int64_t quantity, std::string_view operationId);
    StoreState& state_;
};
} // namespace shop
