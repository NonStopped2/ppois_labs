#pragma once
#include "TransportFixtures.hpp"
#include "post/delivery/Courier.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/PostalException.hpp"
#include "post/payments/CashRegister.hpp"

namespace post::test {
inline StoragePolicy deliveryPolicy(std::int64_t attempts = 2, std::int64_t retryHours = 0) {
    return StoragePolicy("delivery", 0, 2, Money(50), attempts, retryHours);
}
inline Courier courier(std::int64_t capacity = 1000) {
    return Courier(employee("courier", "A", EmployeeRole::Courier), "bike", capacity);
}
inline PostalBranch deliveryBranch(std::int64_t capacity = 10) {
    WorkShift courierShift("courier-shift", "courier", "A", moment(0), moment(10000));
    courierShift.start(employee("courier", "A", EmployeeRole::Courier), "A", moment(0));
    return PostalBranch("A", "Отделение", customer().address(), {storageArea("area", capacity)},
                        {employee(), employee("courier", "A", EmployeeRole::Courier)},
                        {shift(), courierShift}, {"22"});
}
inline std::string acceptCourierParcel(ShipmentRegistry& registry, PostalBranch& branch,
                                       const StoragePolicy& policy = deliveryPolicy(),
                                       std::int64_t weight = 500) {
    Parcel parcel(customer("sender"), customer("recipient"), weight,
                  DeliveryOptions(false, Money{}, false, true), Dimensions(200, 100, 50),
                  PackagingKind::Box);
    parcel.addItem(ContentItem("Книга", 1, 100, Money(100), ContentCategory::GeneralGoods));
    parcel.seal("package-seal");
    const auto id = registry.registerShipment(parcel, 1000, Dimensions(1000, 1000, 1000), 100);
    const PostageQuote quote("quote-" + id, id, registry.at(id).revision(), "plan", moment(10),
                             moment(1000),
                             {InvoiceLine(ServiceKind::BasePostage, "Пересылка", 1, Money(100)),
                              InvoiceLine(ServiceKind::CourierDelivery, "Курьер", 1, Money(20))});
    Invoice invoice("bill-" + id, quote, moment(10), moment(1000));
    invoice.registerPayment(
        Payment("pay-" + id, invoice.id(), Money(120), PaymentMethod::Cash, moment(10)),
        registry.at(id));
    branch.acceptShipment(registry, invoice, id, "worker", policy, 1000,
                          Dimensions(1000, 1000, 1000), 100, moment(15));
    return id;
}
inline DeliveryTask deliveryTask(const std::string& key, const ShipmentRegistry& registry,
                                 const PostalBranch& branch, const std::string& id,
                                 const Courier& actor,
                                 DeliveryTask::Window window = {moment(20), moment(100)}) {
    DeliveryTask result(key, registry.at(id), branch.storageEntry(id));
    result.schedule(window, actor, registry.at(id), deliveryPolicy());
    return result;
}
inline SearchRequest confirmLost(ShipmentRegistry& registry, PostalBranch& source,
                                 const std::string& id) {
    SearchRequest search("search");
    search.open(registry.at(id), registry.at(id).sender(), route(), moment(20));
    search.recordBranchCheck(source, "worker", false, "Не найдено", moment(21));
    search.recordBranchCheck(branch("B", "23"), "worker", false, "Не поступало", moment(22));
    search.closeAsLost(source, registry, "worker", std::chrono::seconds{0}, moment(23));
    return search;
}
} // namespace post::test
