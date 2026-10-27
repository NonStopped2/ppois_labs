#include "../StaffFixtures.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include <catch2/catch_test_macros.hpp>
#include <set>

using namespace post;
namespace {
bool expected(EmployeeRole role, BranchOperation operation) {
    if (role == EmployeeRole::Manager || operation == BranchOperation::StartShift) {
        return true;
    }
    const std::set<std::pair<EmployeeRole, BranchOperation>> allowed{
        {EmployeeRole::Clerk, BranchOperation::AcceptShipment},
        {EmployeeRole::Clerk, BranchOperation::IssueShipment},
        {EmployeeRole::Clerk, BranchOperation::ReceiveBatch},
        {EmployeeRole::Sorter, BranchOperation::Dispatch},
        {EmployeeRole::Sorter, BranchOperation::ReceiveBatch},
        {EmployeeRole::Courier, BranchOperation::DeliverShipment},
        {EmployeeRole::Driver, BranchOperation::DriveVehicle},
        {EmployeeRole::Inspector, BranchOperation::InspectVehicle},
        {EmployeeRole::Inspector, BranchOperation::InvestigateShipment}};
    return allowed.contains({role, operation});
}
} // namespace
TEST_CASE("Employee checks all role and operation combinations", "[employee]") {
    for (int role = 0; role <= static_cast<int>(EmployeeRole::Manager); ++role) {
        const auto employee = test::employee("w", "A", static_cast<EmployeeRole>(role));
        for (int op = 0; op <= static_cast<int>(BranchOperation::InvestigateShipment); ++op) {
            const auto operation = static_cast<BranchOperation>(op);
            if (expected(employee.role(), operation)) {
                REQUIRE_NOTHROW(employee.authorize(operation, " A "));
            } else {
                REQUIRE_THROWS_AS(employee.authorize(operation, "A"),
                                  DeliveryAuthorizationException);
            }
            REQUIRE_THROWS_AS(employee.authorize(operation, "B"), DeliveryAuthorizationException);
        }
    }
}
TEST_CASE("Employee owns validated card data and inactive reason", "[employee]") {
    const Employee employee(" w ", " Имя  Фамилия ", EmployeeRole::Clerk, " A ");
    REQUIRE(employee.id() == "w");
    REQUIRE(employee.fullName() == "Имя Фамилия");
    REQUIRE(employee.branchId() == "A");
    REQUIRE(employee.active());
    REQUIRE(employee.suspensionReason().empty());
    const Employee suspended("s", "Имя", EmployeeRole::Manager, "A", false, " Отстранён ");
    REQUIRE(suspended.suspensionReason() == "Отстранён");
    REQUIRE_THROWS_AS(suspended.authorize(BranchOperation::StartShift, "A"),
                      DeliveryAuthorizationException);
    REQUIRE(employee == employee);
    REQUIRE(employee != suspended);
    REQUIRE_THROWS_AS(employee.authorize(static_cast<BranchOperation>(99), "A"),
                      std::invalid_argument);
}
TEST_CASE("Employee rejects inconsistent cards", "[employee]") {
    REQUIRE_THROWS_AS(Employee("w", " ", EmployeeRole::Clerk, "A"), std::invalid_argument);
    REQUIRE_THROWS_AS(Employee("w", "Имя", static_cast<EmployeeRole>(99), "A"),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(Employee("w", "Имя", EmployeeRole::Clerk, "A", false), std::invalid_argument);
    REQUIRE_THROWS_AS(Employee("w", "Имя", EmployeeRole::Clerk, "A", true, "Причина"),
                      std::invalid_argument);
}
