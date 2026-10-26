#pragma once

#include "post/core/Money.hpp"
#include "post/shipments/ServiceKind.hpp"
#include "post/shipments/ShipmentKind.hpp"

#include <vector>

namespace post {

// Условия отправки: проверяет совместимость опций для письма или посылки.
// Определяет необходимые платные услуги без повторов, но сам не рассчитывает тариф.
class DeliveryOptions {
public:
    explicit DeliveryOptions(bool registered = false, Money declaredValue = Money{},
                             bool identityCheckRequired = false, bool courierDelivery = false,
                             bool fragile = false, bool notificationRequired = false);

    [[nodiscard]] bool registered() const noexcept;
    [[nodiscard]] const Money& declaredValue() const noexcept;
    [[nodiscard]] bool identityCheckRequired() const noexcept;
    [[nodiscard]] bool courierDelivery() const noexcept;
    [[nodiscard]] bool fragile() const noexcept;
    [[nodiscard]] bool notificationRequired() const noexcept;

    void validateCombination(ShipmentKind kind) const;
    [[nodiscard]] std::vector<ServiceKind> requiredServices() const;

    bool operator==(const DeliveryOptions& other) const = default;

private:
    bool registered_;
    Money declaredValue_;
    bool identityCheckRequired_;
    bool courierDelivery_;
    bool fragile_;
    bool notificationRequired_;
};

} // namespace post
