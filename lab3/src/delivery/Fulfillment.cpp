#include "shop/delivery/Fulfillment.hpp"
#include "shop/common/FulfillmentStateException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
Fulfillment::Fulfillment(std::string_view id, const Order& order, std::string_view warehouseId)
    : id_(detail::text(id, "исполнение")), orderId_(order.id()),
      warehouseId_(detail::text(warehouseId, "склад исполнения")),
      requiredQuantities_(order.quantities()) {
    detail::check<FulfillmentStateException>(order.status() == OrderStatus::Confirmed,
                                             "Комплектация требует оплаченного заказа");
    for (const auto& [sku, quantity] : requiredQuantities_) {
        pickedQuantities_.emplace(sku, 0);
    }
}
const std::string& Fulfillment::id() const noexcept {
    return id_;
}
const std::string& Fulfillment::orderId() const noexcept {
    return orderId_;
}
const std::string& Fulfillment::warehouseId() const noexcept {
    return warehouseId_;
}
const std::map<std::string, std::int64_t>& Fulfillment::requiredQuantities() const noexcept {
    return requiredQuantities_;
}
const std::map<std::string, std::int64_t>& Fulfillment::pickedQuantities() const noexcept {
    return pickedQuantities_;
}
FulfillmentStatus Fulfillment::status() const noexcept {
    return status_;
}
const std::optional<std::string>& Fulfillment::trackingNumber() const noexcept {
    return trackingNumber_;
}
const std::optional<Timestamp>& Fulfillment::deliveredAt() const noexcept {
    return deliveredAt_;
}
void Fulfillment::startPicking() {
    detail::check<FulfillmentStateException>(status_ == FulfillmentStatus::Created,
                                             "Подбор уже начат или завершён");
    status_ = FulfillmentStatus::Picking;
}
void Fulfillment::pick(std::string_view sku, std::int64_t quantity) {
    const auto key = std::string(sku);
    const auto required = detail::get(requiredQuantities_, key);
    const auto picked = detail::get(pickedQuantities_, key);
    detail::check<FulfillmentStateException>(status_ == FulfillmentStatus::Picking &&
                                                 quantity > 0 && quantity <= required - picked,
                                             "Нельзя подобрать сверх заказа или вне подбора");
    pickedQuantities_.at(key) += quantity;
}
void Fulfillment::undoPick(std::string_view sku, std::int64_t quantity) {
    const auto key = std::string(sku);
    const auto picked = detail::get(pickedQuantities_, key);
    detail::check<FulfillmentStateException>(status_ == FulfillmentStatus::Picking &&
                                                 quantity > 0 && quantity <= picked,
                                             "Нельзя отменить отсутствующий подбор");
    pickedQuantities_.at(key) -= quantity;
}
void Fulfillment::pack() {
    detail::check<FulfillmentStateException>(status_ == FulfillmentStatus::Picking &&
                                                 requiredQuantities_ == pickedQuantities_,
                                             "Упаковка требует полного подбора");
    status_ = FulfillmentStatus::Packed;
}
void Fulfillment::reopenPicking() {
    detail::check<FulfillmentStateException>(
        status_ == FulfillmentStatus::Packed,
        "Распаковка разрешена до отправки упакованного заказа");
    status_ = FulfillmentStatus::Picking;
}
void Fulfillment::handover(std::string_view trackingValue) {
    std::optional<std::string> tracking = detail::text(trackingValue, "трек-номер");
    detail::check<FulfillmentStateException>(status_ == FulfillmentStatus::Packed,
                                             "Передача требует упаковки");
    trackingNumber_.swap(tracking);
    status_ = FulfillmentStatus::HandedOver;
}
void Fulfillment::deliver(Timestamp now) {
    detail::require(now >= 0, "Некорректное время получения");
    detail::check<FulfillmentStateException>(status_ == FulfillmentStatus::HandedOver,
                                             "Нет отправки для получения");
    deliveredAt_ = now;
    status_ = FulfillmentStatus::Delivered;
}
void Fulfillment::cancel() {
    detail::check<FulfillmentStateException>(status_ == FulfillmentStatus::Created ||
                                                 status_ == FulfillmentStatus::Picking ||
                                                 status_ == FulfillmentStatus::Packed,
                                             "Нельзя отменить переданное исполнение");
    status_ = FulfillmentStatus::Cancelled;
}
} // namespace shop
