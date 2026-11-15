#include "shop/delivery/DeliveryPolicy.hpp"
#include "shop/common/DeliveryRuleException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"
#include <optional>

namespace shop {
DeliveryPolicy::DeliveryPolicy(
    const std::vector<DeliveryOption>& choices, std::int64_t handlingDuration,
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> transitDurations)
    : handlingDuration_(handlingDuration), transitDurationByOption_(std::move(transitDurations)) {
    detail::require(handlingDuration >= 0 && !choices.empty(), "Не заданы способы доставки");
    for (const auto& entry : choices) {
        detail::check<DuplicateEntityException>(options_.emplace(entry.id(), entry).second,
                                                "Повторный способ доставки");
        const auto duration = detail::get(transitDurationByOption_, entry.id());
        detail::require(duration.first >= 0 && duration.second > duration.first,
                        "Некорректный интервал доставки");
    }
    detail::require(transitDurationByOption_.size() == options_.size(),
                    "Лишние интервалы доставки");
}
std::vector<DeliveryOption>
DeliveryPolicy::eligibleOptions(const Address& destination,
                                const std::vector<QuoteLine>& lines) const {
    std::vector<DeliveryOption> result;
    for (const auto& [id, entry] : options_) {
        if (entry.accepts(destination, lines)) {
            result.push_back(entry);
        }
    }
    return result;
}
DeliveryOption DeliveryPolicy::cheapestEligible(const Address& destination,
                                                const std::vector<QuoteLine>& lines,
                                                Money goodsNet) const {
    std::optional<DeliveryOption> chosen;
    for (const auto& entry : eligibleOptions(destination, lines)) {
        if (!chosen || entry.calculateFee(goodsNet) < chosen->calculateFee(goodsNet)) {
            chosen = entry;
        }
    }
    detail::check<DeliveryRuleException>(chosen.has_value(), "Нет доступного способа доставки");
    return *chosen;
}
TimeWindow DeliveryPolicy::estimateArrival(std::string_view optionId, Timestamp now) const {
    const auto duration = detail::get(transitDurationByOption_, optionId);
    const auto prepared = detail::addNonnegative(now, handlingDuration_);
    return TimeWindow(detail::addNonnegative(prepared, duration.first),
                      detail::addNonnegative(prepared, duration.second));
}
DeliveryOption DeliveryPolicy::option(std::string_view id) const {
    return detail::get(options_, id);
}
const std::map<std::string, DeliveryOption>& DeliveryPolicy::options() const noexcept {
    return options_;
}
std::int64_t DeliveryPolicy::handlingDuration() const noexcept {
    return handlingDuration_;
}
const std::map<std::string, std::pair<std::int64_t, std::int64_t>>&
DeliveryPolicy::transitDurations() const noexcept {
    return transitDurationByOption_;
}
} // namespace shop
