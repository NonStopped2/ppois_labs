#pragma once
#include "post/personnel/BranchOperation.hpp"
#include "post/personnel/EmployeeRole.hpp"
#include <string>

namespace post {
// Employee — карточка сотрудника: ID, имя, роль, отделение и признак активности.
// authorize() проверяет право на вид операции с учётом роли и места работы.
// Рабочее время проверяется отдельно через WorkShift внутри PostalBranch.
class Employee {
public:
    Employee(const std::string& id, const std::string& fullName, EmployeeRole role,
             const std::string& branchId, bool active = true,
             const std::string& suspensionReason = "");
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& fullName() const noexcept;
    [[nodiscard]] EmployeeRole role() const noexcept;
    [[nodiscard]] const std::string& branchId() const noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] const std::string& suspensionReason() const noexcept;
    void authorize(BranchOperation operation, const std::string& targetBranchId) const;
    bool operator==(const Employee& other) const = default;

private:
    std::string id_;
    std::string fullName_;
    EmployeeRole role_;
    std::string branchId_;
    bool active_;
    std::string suspensionReason_;
};
} // namespace post
