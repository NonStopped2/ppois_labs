#pragma once
#include "LogisticsFixtures.hpp"
#include "post/delivery/PickupAuthorization.hpp"
#include "post/personnel/Employee.hpp"
#include "post/personnel/WorkShift.hpp"
#include "post/sorting/SortingPlan.hpp"

namespace post::test {
inline Employee employee(const std::string& id = "worker", const std::string& branch = "A",
                         EmployeeRole role = EmployeeRole::Manager) {
    return Employee(id, "Сотрудник Тестов", role, branch);
}
inline WorkShift shift(const std::string& id = "shift", const std::string& branch = "A") {
    WorkShift result(id, "worker", branch, moment(0), moment(10000));
    result.start(employee("worker", branch), branch, moment(0));
    return result;
}
inline IdentityDocument document(const std::string& holder = "Алексей Тестов") {
    using namespace std::chrono;
    return IdentityDocument(DocumentKind::Passport, "AB123", holder, year{1970} / January / 1,
                            year{2100} / December / 31);
}
inline SortingPlan sortingPlan(const std::string& branch = "A", const std::string& target = "B") {
    return SortingPlan("sorting-" + branch, branch,
                       {SortingRule("rule", {"22", "23"}, target, 1,
                                    {ShipmentKind::Letter, ShipmentKind::Parcel})},
                       1);
}
} // namespace post::test
