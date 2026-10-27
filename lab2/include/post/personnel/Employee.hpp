#pragma once
#include "post/personnel/BranchOperation.hpp"
#include "post/personnel/EmployeeRole.hpp"
#include <string>

namespace post {
// Проверенная карточка сотрудника; полномочия зависят от роли, активности и места работы.
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
