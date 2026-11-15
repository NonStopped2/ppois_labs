#pragma once
#include "shop/common/TimeWindow.hpp"
#include <map>
#include <string>
#include <string_view>

namespace shop {
enum class ReservationStatus { Held, Confirmed, Consumed, Released, Expired };
class StockReservation {
  public:
    StockReservation(std::string_view id, std::string_view orderId, std::string_view warehouseId,
                     std::map<std::string, std::int64_t> quantities, Timestamp createdAt,
                     Timestamp expiresAt);
    const std::string& id() const noexcept;
    const std::string& orderId() const noexcept;
    const std::string& warehouseId() const noexcept;
    const std::map<std::string, std::int64_t>& quantities() const noexcept;
    Timestamp createdAt() const noexcept;
    Timestamp expiresAt() const noexcept;
    Timestamp changedAt() const noexcept;
    ReservationStatus status() const noexcept;
    bool live() const noexcept;
    void confirm(Timestamp now);
    void consume(Timestamp now);
    void release(Timestamp now);
    void expire(Timestamp now);
    void extend(Timestamp now, Timestamp newDeadline);
    bool operator==(const StockReservation&) const = default;

  private:
    void checkTime(Timestamp now) const;
    std::string id_;
    std::string orderId_;
    std::string warehouseId_;
    std::map<std::string, std::int64_t> quantities_;
    Timestamp createdAt_;
    Timestamp expiresAt_;
    ReservationStatus status_ = ReservationStatus::Held;
    Timestamp changedAt_;
};
} // namespace shop
