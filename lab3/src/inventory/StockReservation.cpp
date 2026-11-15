#include "shop/inventory/StockReservation.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/ReservationStateException.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
StockReservation::StockReservation(std::string_view id, std::string_view orderId,
                                   std::string_view warehouseId,
                                   std::map<std::string, std::int64_t> quantities,
                                   Timestamp createdAt, Timestamp expiresAt)
    : id_(detail::text(id, "резерв")), orderId_(detail::text(orderId, "заказ")),
      warehouseId_(detail::text(warehouseId, "склад")), quantities_(std::move(quantities)),
      createdAt_(createdAt), expiresAt_(expiresAt), changedAt_(createdAt) {
    detail::require(createdAt >= 0 && expiresAt > createdAt && !quantities_.empty(),
                    "Некорректный резерв");
    for (const auto& [sku, amount] : quantities_) {
        detail::require(detail::text(sku, "SKU") == sku && amount > 0,
                        "Некорректная позиция резерва");
    }
}
const std::string& StockReservation::id() const noexcept {
    return id_;
}
const std::string& StockReservation::orderId() const noexcept {
    return orderId_;
}
const std::string& StockReservation::warehouseId() const noexcept {
    return warehouseId_;
}
const std::map<std::string, std::int64_t>& StockReservation::quantities() const noexcept {
    return quantities_;
}
Timestamp StockReservation::createdAt() const noexcept {
    return createdAt_;
}
Timestamp StockReservation::expiresAt() const noexcept {
    return expiresAt_;
}
Timestamp StockReservation::changedAt() const noexcept {
    return changedAt_;
}
ReservationStatus StockReservation::status() const noexcept {
    return status_;
}
bool StockReservation::live() const noexcept {
    return status_ == ReservationStatus::Held || status_ == ReservationStatus::Confirmed;
}
void StockReservation::checkTime(Timestamp now) const {
    detail::check<ReservationStateException>(now >= changedAt_, "Нарушена хронология резерва");
}
void StockReservation::confirm(Timestamp now) {
    checkTime(now);
    detail::check<ReservationStateException>(status_ == ReservationStatus::Held && now < expiresAt_,
                                             "Неоплаченный резерв недоступен");
    status_ = ReservationStatus::Confirmed;
    changedAt_ = now;
}
void StockReservation::consume(Timestamp now) {
    checkTime(now);
    detail::check<ReservationStateException>(status_ == ReservationStatus::Confirmed,
                                             "Отгрузка требует оплаченного резерва");
    status_ = ReservationStatus::Consumed;
    changedAt_ = now;
}
void StockReservation::release(Timestamp now) {
    checkTime(now);
    detail::check<ReservationStateException>(live(), "Резерв уже завершён");
    status_ = ReservationStatus::Released;
    changedAt_ = now;
}
void StockReservation::expire(Timestamp now) {
    checkTime(now);
    detail::check<ReservationStateException>(status_ == ReservationStatus::Held &&
                                                 now >= expiresAt_,
                                             "Резерв ещё не истёк или уже оплачен");
    status_ = ReservationStatus::Expired;
    changedAt_ = now;
}
void StockReservation::extend(Timestamp now, Timestamp newDeadline) {
    checkTime(now);
    detail::check<ReservationStateException>(status_ == ReservationStatus::Held &&
                                                 now < expiresAt_ && newDeadline > expiresAt_,
                                             "Нельзя продлить этот резерв");
    expiresAt_ = newDeadline;
    changedAt_ = now;
}
} // namespace shop
