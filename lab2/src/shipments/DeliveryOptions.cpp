#include "post/shipments/DeliveryOptions.hpp"

#include "post/exceptions/IncompatibleServiceException.hpp"

namespace post {

DeliveryOptions::DeliveryOptions(bool registered, Money declaredValue, bool identityCheckRequired,
                                 bool courierDelivery, bool fragile, bool notificationRequired)
    : registered_(registered), declaredValue_(declaredValue),
      identityCheckRequired_(identityCheckRequired), courierDelivery_(courierDelivery),
      fragile_(fragile), notificationRequired_(notificationRequired) {
    if (registered_ != identityCheckRequired_ ||
        (!registered_ && (declaredValue_.kopecks() > 0 || notificationRequired_))) {
        throw IncompatibleServiceException(
            "Registration requires identity checks; value and notification require registration");
    }
}

bool DeliveryOptions::registered() const noexcept {
    return registered_;
}

const Money& DeliveryOptions::declaredValue() const noexcept {
    return declaredValue_;
}

bool DeliveryOptions::identityCheckRequired() const noexcept {
    return identityCheckRequired_;
}

bool DeliveryOptions::courierDelivery() const noexcept {
    return courierDelivery_;
}

bool DeliveryOptions::fragile() const noexcept {
    return fragile_;
}

bool DeliveryOptions::notificationRequired() const noexcept {
    return notificationRequired_;
}

void DeliveryOptions::validateCombination(ShipmentKind kind) const {
    if (kind != ShipmentKind::Letter && kind != ShipmentKind::Parcel) {
        throw IncompatibleServiceException("Unknown shipment kind");
    }
    if (kind == ShipmentKind::Letter && (courierDelivery_ || fragile_)) {
        throw IncompatibleServiceException(
            "Courier delivery and fragile handling are parcel-only services");
    }
}

std::vector<ServiceKind> DeliveryOptions::requiredServices() const {
    std::vector<ServiceKind> services;
    // Каждое условие добавляет свою услугу один раз; проверка документа уже включена.
    if (registered_) {
        services.push_back(ServiceKind::Registration);
    }
    if (declaredValue_.kopecks() > 0) {
        services.push_back(ServiceKind::DeclaredValue);
    }
    if (courierDelivery_) {
        services.push_back(ServiceKind::CourierDelivery);
    }
    if (fragile_) {
        services.push_back(ServiceKind::FragileHandling);
    }
    if (notificationRequired_) {
        services.push_back(ServiceKind::DeliveryNotification);
    }
    return services;
}

} // namespace post
