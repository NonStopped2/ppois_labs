#pragma once
#include "post/shipments/Shipment.hpp"
#include <vector>

namespace post {
// SortingRule — одно правило направления: префиксы индексов, виды груза и следующее отделение.
// matches() проверяет отправление; conflictsWith() выявляет неоднозначные правила.
// SortingPlan выбирает подходящее правило с наибольшим приоритетом.
class SortingRule {
public:
    SortingRule(const std::string& id, std::vector<std::string> destinationPrefixes,
                const std::string& nextBranchId, std::int64_t priority,
                std::vector<ShipmentKind> allowedShipmentKinds);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::vector<std::string>& destinationPrefixes() const noexcept;
    [[nodiscard]] const std::string& nextBranchId() const noexcept;
    [[nodiscard]] std::int64_t priority() const noexcept;
    [[nodiscard]] const std::vector<ShipmentKind>& allowedShipmentKinds() const noexcept;
    [[nodiscard]] bool matches(const Shipment& shipment) const;
    [[nodiscard]] bool conflictsWith(const SortingRule& other) const;
    bool operator==(const SortingRule& other) const = default;

private:
    std::string id_;
    std::vector<std::string> destinationPrefixes_;
    std::string nextBranchId_;
    std::int64_t priority_;
    std::vector<ShipmentKind> allowedShipmentKinds_;
};
} // namespace post
