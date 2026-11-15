#pragma once

#include "shop/orders/Order.hpp"
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace shop {
/** Immutable shared snapshots keep previously returned orders alive across replacements. */
class OrderBook {
  public:
    void add(const Order& order);
    void replace(const Order& order);
    std::shared_ptr<const Order> order(std::string_view id) const;
    const std::map<std::string, std::shared_ptr<const Order>>& orders() const noexcept;
    std::int64_t revision() const noexcept;
    std::vector<std::string> findOverdueUnpaid(Timestamp now) const;
    std::vector<std::shared_ptr<const Order>>
    customerHistory(std::string_view customerId, std::optional<OrderStatus> status = std::nullopt,
                    std::optional<TimeWindow> period = std::nullopt) const;
    std::vector<std::shared_ptr<const Order>> selectForPeriod(const TimeWindow& period) const;
    bool operator==(const OrderBook& other) const;

  private:
    friend class SnapshotCodec;
    std::map<std::string, std::shared_ptr<const Order>> orders_;
    std::map<std::string, std::set<std::string>> customerOrderIndex_;
    std::int64_t revision_ = 0;
};
} // namespace shop
