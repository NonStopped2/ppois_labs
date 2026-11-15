#pragma once
#include "post/delivery/PickupAuthorization.hpp"
#include "post/detail/Time.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
namespace post::detail {
// Общие правила личности для выдачи в отделении и вручения курьером.
inline void validateRecipient(const Shipment& shipment, const Customer& claimant,
                              const IdentityDocument& document,
                              const PickupAuthorization* authorization,
                              std::chrono::sys_seconds time) {
    const auto& principal =
        shipment.status() == ShipmentStatus::Returning ? shipment.sender() : shipment.recipient();
    if (claimant.id() == principal.id()) {
        document.validateForHolder(principal.fullName(), dateAt(time));
    } else {
        if (!authorization) {
            throw DeliveryAuthorizationException("Representative needs pickup authorization");
        }
        authorization->validateFor(shipment, principal, claimant, document, time);
    }
}
} // namespace post::detail
