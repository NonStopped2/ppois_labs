#pragma once
#include "post/sorting/SortingRule.hpp"
#include <functional>
#include <map>

namespace post {
// Неизменяемая версия правил одного отделения; группировка возвращает номера, не копии отправлений.
class SortingPlan {
public:
    SortingPlan(const std::string& id, const std::string& branchId, std::vector<SortingRule> rules,
                std::int64_t version);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& branchId() const noexcept;
    [[nodiscard]] const std::vector<SortingRule>& rules() const noexcept;
    [[nodiscard]] std::int64_t version() const noexcept;
    void validateRules() const;
    [[nodiscard]] std::map<std::string, std::vector<std::string>>
    groupForDispatch(const std::vector<std::reference_wrapper<const Shipment>>& shipments) const;
    bool operator==(const SortingPlan& other) const = default;

private:
    [[nodiscard]] std::string directionFor(const Shipment& shipment) const;
    std::string id_;
    std::string branchId_;
    std::vector<SortingRule> rules_;
    std::int64_t version_;
};
} // namespace post
