#include "post/personnel/Employee.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"

namespace post {
namespace {
// StartShift доступна любой известной роли; остальные действия разделяют обязанности.
bool permitted(EmployeeRole role, BranchOperation operation) {
    if (operation == BranchOperation::StartShift || role == EmployeeRole::Manager) {
        return true;
    }
    switch (role) {
    case EmployeeRole::Clerk:
        return operation == BranchOperation::AcceptShipment ||
               operation == BranchOperation::IssueShipment ||
               operation == BranchOperation::ReceiveBatch;
    case EmployeeRole::Sorter:
        return operation == BranchOperation::Dispatch || operation == BranchOperation::ReceiveBatch;
    case EmployeeRole::Courier:
        return operation == BranchOperation::DeliverShipment;
    case EmployeeRole::Driver:
        return operation == BranchOperation::DriveVehicle;
    case EmployeeRole::Inspector:
        return operation == BranchOperation::InspectVehicle ||
               operation == BranchOperation::InvestigateShipment;
    default:
        return false;
    }
}
} // namespace
// У неактивного сотрудника обязательно основание, у активного — нет ложной причины отстранения.
Employee::Employee(const std::string& id, const std::string& fullName, EmployeeRole role,
                   const std::string& branchId, bool active, const std::string& suspensionReason)
    : id_(detail::identifier(id)), fullName_(detail::normalizeText(fullName)), role_(role),
      branchId_(detail::identifier(branchId)), active_(active),
      suspensionReason_(detail::normalizeText(suspensionReason)) {
    if (!detail::hasPrintableText(fullName_) || role_ < EmployeeRole::Clerk ||
        role_ > EmployeeRole::Manager || active_ != suspensionReason_.empty() ||
        (!suspensionReason_.empty() && !detail::hasPrintableText(suspensionReason_))) {
        throw std::invalid_argument("Invalid employee card");
    }
}
// Идентификатор используется при назначении смен и аудите полномочий.
const std::string& Employee::id() const noexcept {
    return id_;
}
// Имя хранится для отображения, не как ключ полномочий.
const std::string& Employee::fullName() const noexcept {
    return fullName_;
}
// Роль не определяется по названию должности в произвольном тексте.
EmployeeRole Employee::role() const noexcept {
    return role_;
}
// Полномочия ограничены одним местом работы.
const std::string& Employee::branchId() const noexcept {
    return branchId_;
}
// Неактивность отменяет любые разрешения роли.
bool Employee::active() const noexcept {
    return active_;
}
// Причина сохраняется для объяснения отказа, а не заменяет флаг активности.
const std::string& Employee::suspensionReason() const noexcept {
    return suspensionReason_;
}
// Не принимает весь PostalBranch: проверке действительно нужен только его идентификатор.
void Employee::authorize(BranchOperation operation, const std::string& targetBranchId) const {
    const auto target = detail::identifier(targetBranchId);
    if (operation < BranchOperation::StartShift ||
        operation > BranchOperation::InvestigateShipment) {
        throw std::invalid_argument("Unknown branch operation");
    }
    if (!active_ || target != branchId_ || !permitted(role_, operation)) {
        throw DeliveryAuthorizationException("Employee lacks permission at this branch");
    }
}
} // namespace post
