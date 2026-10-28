#include "post/sorting/SortingPlan.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include "post/detail/Validation.hpp"
#include <algorithm>
#include <iterator>
#include <set>

namespace post {
// Проверка конфигурации выполняется сразу, а не при первом спорном отправлении.
SortingPlan::SortingPlan(const std::string& id, const std::string& branchId,
                         std::vector<SortingRule> rules, std::int64_t version)
    : id_(detail::identifier(id)), branchId_(detail::identifier(branchId)),
      rules_(std::move(rules)), version_(version) {
    validateRules();
}
// Идентификатор отличает план от его числовой версии.
const std::string& SortingPlan::id() const noexcept {
    return id_;
}
// Правила применяются только к отправлениям, находящимся в этом отделении.
const std::string& SortingPlan::branchId() const noexcept {
    return branchId_;
}
// Внешний код не может создать конфликт через изменяемый геттер.
const std::vector<SortingRule>& SortingPlan::rules() const noexcept {
    return rules_;
}
// Версия сохраняет объяснимость выбранного набора правил.
std::int64_t SortingPlan::version() const noexcept {
    return version_;
}
// Запрещает повторные ключи, возврат в себя и конфликт покрытия одинакового приоритета.
void SortingPlan::validateRules() const {
    if (rules_.empty() || version_ <= 0) {
        throw std::invalid_argument("Sorting plan needs rules and positive version");
    }
    std::set<std::string> ids;
    for (auto current = rules_.begin(); current != rules_.end(); ++current) {
        if (current->nextBranchId() == branchId_ || !ids.insert(current->id()).second) {
            throw std::invalid_argument("Invalid sorting direction or duplicate rule");
        }
        if (std::any_of(std::next(current), rules_.end(),
                        [&current](const auto& other) { return current->conflictsWith(other); })) {
            throw std::invalid_argument("Ambiguous sorting rules");
        }
    }
}
// Выбирает максимальный приоритет; защитная проверка не полагается только на конструктор.
std::string SortingPlan::directionFor(const Shipment& shipment) const {
    const SortingRule* selected = nullptr;
    for (const auto& rule : rules_) {
        if (!rule.matches(shipment)) {
            continue;
        }
        if (!selected || rule.priority() > selected->priority()) {
            selected = &rule;
        } else if (rule.priority() == selected->priority() &&
                   rule.nextBranchId() != selected->nextBranchId()) {
            throw InvalidShipmentStateException("Ambiguous sorting result");
        }
    }
    if (!selected) {
        throw InvalidShipmentStateException("No sorting rule for shipment");
    }
    return selected->nextBranchId();
}
// Сначала проверяет весь набор; возвращаемый план не перемещает ни одного отправления.
std::map<std::string, std::vector<std::string>> SortingPlan::groupForDispatch(
    const std::vector<std::reference_wrapper<const Shipment>>& shipments) const {
    std::map<std::string, std::vector<std::string>> result;
    std::set<std::string> ids;
    for (const auto reference : shipments) {
        const auto& shipment = reference.get();
        if (!detail::availableForBranchStorage(shipment) ||
            shipment.history().lastKnownBranch() != branchId_ ||
            !ids.insert(shipment.trackingNumber()).second) {
            throw InvalidShipmentStateException("Shipment unavailable for sorting at this branch");
        }
        result[directionFor(shipment)].push_back(shipment.trackingNumber());
    }
    return result;
}
} // namespace post
