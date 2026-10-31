#include "Defaults.hpp"
#include <string_view>

namespace post::cli {
namespace {
// То же покрытие используется в отделениях, зонах тарифа и правилах сортировки.
std::vector<std::string> postalPrefixes() {
    std::vector<std::string> result;
    for (const auto& [id, prefix] : kBranches) {
        (void)id;
        result.emplace_back(prefix);
    }
    return result;
}
// Суммы и процент имеют имена и единицы; ноль фиксированного тарифа означает отсутствие процента.
std::vector<ServiceRate> serviceRates() {
    return {ServiceRate(ServiceKind::Registration, RateMode::Fixed, Money(kRegistrationFee), 0),
            ServiceRate(ServiceKind::DeclaredValue, RateMode::Percentage, Money{},
                        kDeclaredValueBasisPoints),
            ServiceRate(ServiceKind::CourierDelivery, RateMode::Fixed, Money(kCourierFee), 0),
            ServiceRate(ServiceKind::FragileHandling, RateMode::Fixed, Money(kFragileFee), 0),
            ServiceRate(ServiceKind::DeliveryNotification, RateMode::Fixed, Money(kNotificationFee),
                        0)};
}
} // namespace
// Один явный предел используется и при регистрации, и при физическом приёме.
Dimensions sizeLimit() {
    return Dimensions(kMaxSideMm, kMaxSideMm, kMaxSideMm);
}
// Повтор требует часа ожидания и сохраняет дедлайн исходного задания.
StoragePolicy defaultStoragePolicy() {
    return StoragePolicy("storage", kFreeStorageDays, kMaxStorageDays, Money(kDailyStorageFee),
                         kMaxDeliveryAttempts, kMinRetryHours);
}
// Оба тарифа покрывают три учебных района, но различаются базой и шаговой ценой.
PostageCalculator defaultCalculator() {
    std::vector<TariffPlan> plans;
    for (const auto& id : {"standard", "express"}) {
        const bool express = std::string_view(id) == "express";
        plans.emplace_back(id, id, kEpoch, kSessionEnd,
                           std::vector{TariffZone("domestic", "Учебные районы", postalPrefixes(),
                                                  kDomesticBasisPoints)},
                           std::vector{WeightBand(
                               0, kMaxWeight, Money(express ? kExpressBase : kStandardBase),
                               kWeightStepGrams, Money(express ? kExpressStep : kStandardStep))},
                           serviceRates());
    }
    return PostageCalculator(std::move(plans), 1, kQuoteLifetime);
}
// Отделение владеет каноническими карточками и сменами, включая отдельного курьера.
PostalBranch defaultBranch(const std::string& id, const std::string& prefix) {
    const PostalAddress address(prefix + "0000", "Учебный город " + id, "Почтовая", "1");
    std::vector<Employee> employees{
        Employee("manager-" + id, "Руководитель " + id, EmployeeRole::Manager, id),
        Employee("courier-" + id, "Курьер " + id, EmployeeRole::Courier, id)};
    std::vector<WorkShift> shifts;
    for (const auto& employee : employees) {
        WorkShift shift("shift-" + employee.id(), employee.id(), id, kEpoch,
                        kEpoch + kShiftDuration);
        shift.start(employee, id, kEpoch);
        shifts.push_back(std::move(shift));
    }
    const auto cell = StorageCell(id + "-general", kCellWeight, kCellCapacity, sizeLimit());
    const auto fragile = StorageCell(id + "-fragile", kCellWeight, kCellCapacity, sizeLimit());
    return PostalBranch(id, "Отделение " + id, address,
                        {StorageArea("general-" + id, "Общая зона", StorageAreaKind::General,
                                     {cell}, {ShipmentKind::Letter, ShipmentKind::Parcel}),
                         StorageArea("fragile-" + id, "Хрупкое", StorageAreaKind::Fragile,
                                     {fragile}, {ShipmentKind::Parcel})},
                        employees, shifts, {prefix});
}
// Следующий узел выбирает оператор; проверку фактического размещения выполняет отделение.
SortingPlan sortingPlan(const std::string& source, const std::string& target) {
    return SortingPlan("sorting-" + source + "-" + target, source,
                       {SortingRule("direction", postalPrefixes(), target, 1,
                                    {ShipmentKind::Letter, ShipmentKind::Parcel})},
                       1);
}
// Остановки сохраняют введённый порядок; модель отвергает дубли и неверное направление.
PostalRoute postalRoute(const std::string& id, const std::vector<std::string>& branchIds) {
    std::vector<RouteStop> stops;
    for (std::size_t index = 0; index < branchIds.size(); ++index) {
        stops.emplace_back(branchIds[index], static_cast<std::int64_t>(index + 1),
                           index == 0 ? std::chrono::minutes{0} : kTravel, kHandling);
    }
    return PostalRoute(id, "Учебный маршрут", stops, kMaxRouteDuration, {VehicleType::Van});
}
} // namespace post::cli
