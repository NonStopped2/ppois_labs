#pragma once

#include "shop/application/StoreState.hpp"

namespace shop {
class Checkout {
  public:
    explicit Checkout(StoreState& state);
    PriceQuote preview(std::string_view actor, const Address& destination,
                       std::string_view deliveryOptionId) const;
    std::string placeOrder(std::string_view actor, const PriceQuote& acceptedQuote,
                           std::string_view operationId);
    std::vector<std::string> expireUnpaidOrders();
    void moveCartItemsToWishlist(std::string_view actor, const std::vector<std::string>& skus,
                                 std::string_view operationId);

  private:
    StoreState& state_;
};
} // namespace shop
