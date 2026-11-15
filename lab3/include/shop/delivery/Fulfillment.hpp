#pragma once

#include "shop/orders/Order.hpp"
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace shop {
enum class FulfillmentStatus { Created, Picking, Packed, HandedOver, Delivered, Cancelled };
/** A single complete shipment; picking does not yet consume the stock reservation. */
class Fulfillment {
  public:
    Fulfillment(std::string_view id, const Order& order, std::string_view warehouseId);
    const std::string& id() const noexcept;
    const std::string& orderId() const noexcept;
    const std::string& warehouseId() const noexcept;
    const std::map<std::string, std::int64_t>& requiredQuantities() const noexcept;
    const std::map<std::string, std::int64_t>& pickedQuantities() const noexcept;
    FulfillmentStatus status() const noexcept;
    const std::optional<std::string>& trackingNumber() const noexcept;
    const std::optional<Timestamp>& deliveredAt() const noexcept;
    void startPicking();
    void pick(std::string_view sku, std::int64_t quantity);
    void undoPick(std::string_view sku, std::int64_t quantity);
    void pack();
    void reopenPicking();
    void handover(std::string_view trackingNumber);
    void deliver(Timestamp now);
    void cancel();
    bool operator==(const Fulfillment&) const = default;

  private:
    std::string id_;
    std::string orderId_;
    std::string warehouseId_;
    std::map<std::string, std::int64_t> requiredQuantities_;
    std::map<std::string, std::int64_t> pickedQuantities_;
    FulfillmentStatus status_ = FulfillmentStatus::Created;
    std::optional<std::string> trackingNumber_;
    std::optional<Timestamp> deliveredAt_;
};
} // namespace shop
