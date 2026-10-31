#include "post/shipments/DeliveryOptions.hpp"

#include "post/exceptions/IncompatibleServiceException.hpp"

// DeliveryOptions — условия отправки: регистрация, ценность, курьер и другие услуги.
// Проверяет допустимость сочетаний с учётом вида отправления Letter или Parcel.
// Возвращает калькулятору список необходимых услуг без повторов, но не назначает цены.
namespace post {

// Проверяет общие зависимости опций; ограничения вида отправления проверяются отдельно.
// Регистрация и проверка документа включаются совместно, ценность и уведомление требуют их.
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

// Признак регистрации нужен для выбора платной услуги и зависимых условий.
bool DeliveryOptions::registered() const noexcept {
    return registered_;
}

// Ценность отправления для услуги оценки; ноль означает отсутствие объявленной ценности.
const Money& DeliveryOptions::declaredValue() const noexcept {
    return declaredValue_;
}

// Требование проверки получателя; отдельная платная услуга за него не добавляется.
bool DeliveryOptions::identityCheckRequired() const noexcept {
    return identityCheckRequired_;
}

// Выбор доставки курьером вместо обычной выдачи в отделении.
bool DeliveryOptions::courierDelivery() const noexcept {
    return courierDelivery_;
}

// Признак специальной обработки хрупкого, разрешённой только для посылок.
bool DeliveryOptions::fragile() const noexcept {
    return fragile_;
}

// Требование уведомления о вручении для включения соответствующей услуги в расчёт.
bool DeliveryOptions::notificationRequired() const noexcept {
    return notificationRequired_;
}

// Проверяет условия для конкретного вида: письму недоступны курьер и хрупкая обработка.
void DeliveryOptions::validateCombination(ShipmentKind kind) const {
    if (kind != ShipmentKind::Letter && kind != ShipmentKind::Parcel) {
        throw IncompatibleServiceException("Unknown shipment kind");
    }
    if (kind == ShipmentKind::Letter && (courierDelivery_ || fragile_)) {
        throw IncompatibleServiceException(
            "Courier delivery and fragile handling are parcel-only services");
    }
}

// Готовит набор услуг для тарификации, а не рассчитывает или списывает деньги.
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
