#pragma once
#include "post/core/IdentityDocument.hpp"
#include "post/shipments/Shipment.hpp"

namespace post {
// PickupAuthorization — разрешение получить конкретное отправление за другого клиента.
// Хранит ID отправления, доверителя и представителя, срок действия и дату отзыва.
// При выдаче в отделении или курьером сверяет участников, документ, время и отсутствие отзыва.
class PickupAuthorization {
public:
    PickupAuthorization(const std::string& id, const Shipment& shipment, const Customer& principal,
                        const Customer& representative, std::chrono::sys_seconds validFrom,
                        std::chrono::sys_seconds validUntil);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& shipmentId() const noexcept;
    [[nodiscard]] const std::string& principalCustomerId() const noexcept;
    [[nodiscard]] const std::string& representativeCustomerId() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds validFrom() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds validUntil() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> revokedAt() const noexcept;
    void validateFor(const Shipment& shipment, const Customer& principal,
                     const Customer& representative, const IdentityDocument& document,
                     std::chrono::sys_seconds time) const;
    void revoke(std::chrono::sys_seconds time);
    bool operator==(const PickupAuthorization& other) const = default;

private:
    std::string id_;
    std::string shipmentId_;
    std::string principalCustomerId_;
    std::string representativeCustomerId_;
    std::chrono::sys_seconds validFrom_;
    std::chrono::sys_seconds validUntil_;
    std::optional<std::chrono::sys_seconds> revokedAt_;
};
} // namespace post
