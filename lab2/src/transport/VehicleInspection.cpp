#include "post/transport/VehicleInspection.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/TransportUnavailableException.hpp"
#include <algorithm>

namespace post {
// У документа явный включённый срок и конкретная машина, но ещё нет одобрения.
VehicleInspection::VehicleInspection(const std::string& id, const Vehicle& vehicle,
                                     std::chrono::sys_seconds inspectedAt,
                                     std::chrono::sys_seconds expiresAt)
    : id_(detail::identifier(id)), vehicleId_(vehicle.id()), inspectedAt_(inspectedAt),
      expiresAt_(expiresAt) {
    detail::requireTime(inspectedAt_);
    detail::requireTime(expiresAt_);
    if (expiresAt_ < inspectedAt_) {
        throw std::invalid_argument("Invalid inspection interval");
    }
}
// Идентификатор отличает документы одной машины.
const std::string& VehicleInspection::id() const noexcept {
    return id_;
}
// Допуск нельзя переносить на другую машину.
const std::string& VehicleInspection::vehicleId() const noexcept {
    return vehicleId_;
}
// Начальная дата срока включена.
std::chrono::sys_seconds VehicleInspection::inspectedAt() const noexcept {
    return inspectedAt_;
}
// Последний момент срока также включён.
std::chrono::sys_seconds VehicleInspection::expiresAt() const noexcept {
    return expiresAt_;
}
// Сохраняет автора последнего одобрения, даже если оно затем отозвано новым дефектом.
const std::string& VehicleInspection::inspectorId() const noexcept {
    return inspectorId_;
}
// true означает устранённое замечание; наружу доступно только чтение.
const std::map<std::string, bool>& VehicleInspection::defects() const noexcept {
    return defects_;
}
// Исправления сами по себе не восстанавливают одобрение.
bool VehicleInspection::approved() const noexcept {
    return approved_;
}
// Новое замечание всегда снимает прежний допуск.
void VehicleInspection::addDefect(const std::string& description) {
    const auto text = detail::normalizeText(description);
    if (!detail::hasPrintableText(text)) {
        throw std::invalid_argument("Defect needs description");
    }
    if (!defects_.emplace(text, false).second) {
        throw TransportUnavailableException("Defect already recorded");
    }
    approved_ = false;
}
// Неизвестное или уже устранённое замечание не переписывает журнал.
void VehicleInspection::resolveDefect(const std::string& description) {
    const auto found = defects_.find(detail::normalizeText(description));
    if (found == defects_.end() || found->second) {
        throw TransportUnavailableException("Defect not awaiting resolution");
    }
    found->second = true;
}
// Проверяет роль и активность сотрудника, срок и отсутствие неустранённых дефектов.
void VehicleInspection::approve(const Employee& inspector, std::chrono::sys_seconds time) {
    detail::requireTime(time);
    inspector.authorize(BranchOperation::InspectVehicle, inspector.branchId());
    if (approved_ || time < inspectedAt_ || time > expiresAt_ ||
        std::any_of(defects_.begin(), defects_.end(),
                    [](const auto& entry) { return !entry.second; })) {
        throw TransportUnavailableException("Inspection cannot be approved");
    }
    auto actor = inspector.id();
    inspectorId_.swap(actor);
    approved_ = true;
}
// Время и действующий документ проверяются и при резервировании, и перед выездом.
void VehicleInspection::validateFor(const Vehicle& vehicle, std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    if (vehicle.id() != vehicleId_ || !approved_ || time < inspectedAt_ || time > expiresAt_) {
        throw TransportUnavailableException("Vehicle lacks valid inspection");
    }
}
} // namespace post
