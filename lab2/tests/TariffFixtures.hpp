#pragma once
#include "Fixtures.hpp"
#include "post/tariffs/PostageQuote.hpp"
#include "post/tariffs/TariffPlan.hpp"

namespace post::test {
inline std::chrono::sys_seconds moment(std::int64_t value) {
    return std::chrono::sys_seconds{std::chrono::seconds{value}};
}
inline TariffPlan plan(const std::string& id = "plan", Money base = Money(100)) {
    return TariffPlan(
        id, "Учебный тариф", moment(0), moment(1000), {TariffZone("zone", "Минск", {"22"}, 15000)},
        {WeightBand(0, 1000, base, 100, Money(20))},
        {ServiceRate(ServiceKind::Registration, RateMode::Fixed, Money(50), 0),
         ServiceRate(ServiceKind::DeclaredValue, RateMode::Percentage, Money{}, 100),
         ServiceRate(ServiceKind::CourierDelivery, RateMode::Fixed, Money(100), 0),
         ServiceRate(ServiceKind::FragileHandling, RateMode::Fixed, Money(25), 0),
         ServiceRate(ServiceKind::DeliveryNotification, RateMode::Fixed, Money(15), 0)});
}
inline PostageQuote quote(const Shipment& shipment, Money price = Money(100)) {
    return PostageQuote("quote", shipment.trackingNumber(), shipment.revision(), "plan", moment(10),
                        moment(20), {InvoiceLine(ServiceKind::BasePostage, "Пересылка", 1, price)});
}
} // namespace post::test
