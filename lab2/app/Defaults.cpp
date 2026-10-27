#include "Defaults.hpp"
#include <string_view>

namespace post::cli {
// Один явный предел используется и при регистрации, и при физическом приёме.
Dimensions sizeLimit() {
    return Dimensions(1000, 1000, 1000);
}
// Повтор требует часа ожидания и сохраняет дедлайн исходного задания.
StoragePolicy defaultStoragePolicy() {
    return StoragePolicy("storage", 2, 7, Money(50), 3, 1);
}
// Оба тарифа покрывают три учебных района, но различаются базой и шаговой ценой.
PostageCalculator defaultCalculator() {
    std::vector<TariffPlan> plans;
    for (const auto& id : {"standard", "express"}) {
        const bool express = std::string_view(id) == "express";
        plans.emplace_back(
            id, id, kEpoch, kSessionEnd,
            std::vector{TariffZone("domestic", "Учебные районы", {"22", "23", "24"}, 10000)},
            std::vector{WeightBand(0, kMaxWeight, Money(express ? 600 : 300), 1000,
                                   Money(express ? 100 : 75))},
            std::vector{
                ServiceRate(ServiceKind::Registration, RateMode::Fixed, Money(100), 0),
                ServiceRate(ServiceKind::DeclaredValue, RateMode::Percentage, Money{}, 100),
                ServiceRate(ServiceKind::CourierDelivery, RateMode::Fixed, Money(250), 0),
                ServiceRate(ServiceKind::FragileHandling, RateMode::Fixed, Money(100), 0),
                ServiceRate(ServiceKind::DeliveryNotification, RateMode::Fixed, Money(50), 0)});
    }
    return PostageCalculator(std::move(plans), 1, std::chrono::hours{24});
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
    const auto cell = StorageCell(id + "-general", kVehicleWeight, 100, sizeLimit());
    const auto fragile = StorageCell(id + "-fragile", kVehicleWeight, 100, sizeLimit());
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
                       {SortingRule("direction", {"22", "23", "24"}, target, 1,
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
    return PostalRoute(id, "Учебный маршрут", stops, std::chrono::hours{24}, {VehicleType::Van});
}
} // namespace post::cli
