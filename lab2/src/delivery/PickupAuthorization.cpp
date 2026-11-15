#include "post/delivery/PickupAuthorization.hpp"
#include "post/detail/Time.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"

// PickupAuthorization — разрешение получить конкретное отправление за другого клиента.
// Хранит ID отправления, доверителя и представителя, срок действия и дату отзыва.
// При выдаче в отделении или курьером сверяет участников, документ, время и отсутствие отзыва.
namespace post {
namespace {
// При возврате получать отправление имеет право исходный отправитель.
const Customer& principalFor(const Shipment& shipment) {
    return shipment.status() == ShipmentStatus::Returning ? shipment.sender()
                                                          : shipment.recipient();
}
} // namespace
// Сроки включены с обеих сторон; представитель и доверитель должны быть разными клиентами.
PickupAuthorization::PickupAuthorization(const std::string& id, const Shipment& shipment,
                                         const Customer& principal, const Customer& representative,
                                         std::chrono::sys_seconds validFrom,
                                         std::chrono::sys_seconds validUntil)
    : id_(detail::identifier(id)), shipmentId_(detail::identifier(shipment.trackingNumber())),
      principalCustomerId_(principal.id()), representativeCustomerId_(representative.id()),
      validFrom_(validFrom), validUntil_(validUntil) {
    detail::requireTime(validFrom_);
    detail::requireTime(validUntil_);
    if (validUntil_ < validFrom_ || principal.id() != principalFor(shipment).id() ||
        principal.id() == representative.id()) {
        throw DeliveryAuthorizationException("Invalid pickup authorization scope");
    }
}
// Идентификатор позволяет отозвать именно это разрешение.
const std::string& PickupAuthorization::id() const noexcept {
    return id_;
}
// Разрешение не переносится на другое отправление того же клиента.
const std::string& PickupAuthorization::shipmentId() const noexcept {
    return shipmentId_;
}
// Доверитель остаётся снимком идентификатора, не изменяемой внешней карточкой.
const std::string& PickupAuthorization::principalCustomerId() const noexcept {
    return principalCustomerId_;
}
// Личность представителя сверяется отдельно по клиенту и документу.
const std::string& PickupAuthorization::representativeCustomerId() const noexcept {
    return representativeCustomerId_;
}
// Начальная граница включена.
std::chrono::sys_seconds PickupAuthorization::validFrom() const noexcept {
    return validFrom_;
}
// Конечная граница включена.
std::chrono::sys_seconds PickupAuthorization::validUntil() const noexcept {
    return validUntil_;
}
// Отзыв необратим для последующих попыток применения, даже с подставленной старой датой.
std::optional<std::chrono::sys_seconds> PickupAuthorization::revokedAt() const noexcept {
    return revokedAt_;
}
// Совпадение идентификаторов не заменяет проверку состояния и документа представителя.
void PickupAuthorization::validateFor(const Shipment& shipment, const Customer& principal,
                                      const Customer& representative,
                                      const IdentityDocument& document,
                                      std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    const auto state = shipment.status();
    const bool issuable =
        state == ShipmentStatus::Arrived || state == ShipmentStatus::AwaitingPickup ||
        state == ShipmentStatus::OutForDelivery || state == ShipmentStatus::Returning;
    if (revokedAt_ || !issuable || shipment.trackingNumber() != shipmentId_ ||
        principal.id() != principalCustomerId_ || principal.id() != principalFor(shipment).id() ||
        representative.id() != representativeCustomerId_ || time < validFrom_ ||
        time > validUntil_ || time < shipment.history().events().back().occurredAt()) {
        throw DeliveryAuthorizationException("Pickup authorization not applicable");
    }
    document.validateForHolder(representative.fullName(), detail::dateAt(time));
}
// Дату передаёт вызывающий процесс; повторный отзыв не переписывает первоначальный факт.
void PickupAuthorization::revoke(std::chrono::sys_seconds time) {
    detail::requireTime(time);
    if (revokedAt_) {
        throw DeliveryAuthorizationException("Authorization already revoked");
    }
    revokedAt_ = time;
}
} // namespace post
