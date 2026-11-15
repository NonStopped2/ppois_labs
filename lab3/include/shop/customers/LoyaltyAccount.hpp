#pragma once

#include "shop/common/Money.hpp"
#include <map>
#include <string>
#include <string_view>
#include <tuple>

namespace shop {

/** Delivered goods turnover net of completed returns, with replay protection. */
class LoyaltyAccount {
  public:
    explicit LoyaltyAccount(std::string_view customerId);
    const std::string& customerId() const noexcept;
    Money qualifyingSpend() const noexcept;
    const std::map<std::string, Money>& netDeliveredPurchases() const noexcept;
    void recordDelivery(std::string_view orderId, Money goodsNet, std::string_view operationId);
    void recordReturn(std::string_view orderId, Money returned, std::string_view operationId);
    bool operator==(const LoyaltyAccount&) const = default;

  private:
    using Operation = std::tuple<std::string, Money, bool>;
    bool replay(std::string_view operationId, const Operation& operation) const;
    void record(std::string_view orderId, Money amount, std::string_view operationId,
                bool returning);
    std::string customerId_;
    std::map<std::string, Money> netDeliveredPurchases_;
    std::map<std::string, Operation> processedOperations_;
    Money qualifyingSpend_;
};

} // namespace shop
