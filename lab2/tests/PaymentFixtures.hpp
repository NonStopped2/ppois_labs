#pragma once
#include "TariffFixtures.hpp"
#include "post/payments/Invoice.hpp"

namespace post::test {
inline Invoice invoice(const Shipment& shipment, Money price = Money(100)) {
    return Invoice("invoice", quote(shipment, price), moment(10), moment(20));
}
inline Payment payment(const std::string& id, std::int64_t amount, std::int64_t time = 10) {
    return Payment(id, "invoice", Money(amount), PaymentMethod::Cash, moment(time));
}
inline Refund refund(const std::string& id, const std::string& paymentId, std::int64_t amount,
                     std::int64_t time = 10) {
    return Refund(id, paymentId, "invoice", Money(amount), moment(time), "Возврат оплаты");
}
} // namespace post::test
