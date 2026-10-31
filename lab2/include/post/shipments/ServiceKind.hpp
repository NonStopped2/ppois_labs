#pragma once

namespace post {

// Проверка документа входит в регистрацию и не образует вторую платную услугу.
enum class ServiceKind {
    Registration,
    DeclaredValue,
    CourierDelivery,
    FragileHandling,
    DeliveryNotification,
    BasePostage
};

} // namespace post
