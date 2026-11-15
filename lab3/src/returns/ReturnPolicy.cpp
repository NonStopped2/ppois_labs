#include "shop/returns/ReturnPolicy.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/ReturnRuleException.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {
ReturnPolicy::ReturnPolicy(std::int64_t returnWindow, std::set<std::string> nonReturnableCategories,
                           std::set<ReturnReason> allowedReasons,
                           std::set<ItemCondition> restockableConditions)
    : returnWindow_(returnWindow), nonReturnableCategories_(std::move(nonReturnableCategories)),
      allowedReasons_(std::move(allowedReasons)),
      restockableConditions_(std::move(restockableConditions)) {
    detail::require(returnWindow > 0 && !allowedReasons_.empty(), "Некорректная политика возврата");
    for (const auto& id : nonReturnableCategories_) {
        detail::text(id, "невозвратная категория");
    }
    for (const auto reason : allowedReasons_) {
        detail::require(reason == ReturnReason::ChangedMind || reason == ReturnReason::Defective ||
                            reason == ReturnReason::WrongItem,
                        "Неизвестная причина возврата");
    }
    for (const auto condition : restockableConditions_) {
        detail::require(condition == ItemCondition::Unopened || condition == ItemCondition::Opened,
                        "Брак и некомплект нельзя вернуть в доступные остатки");
    }
}
std::int64_t ReturnPolicy::returnWindow() const noexcept {
    return returnWindow_;
}
const std::set<std::string>& ReturnPolicy::nonReturnableCategories() const noexcept {
    return nonReturnableCategories_;
}
const std::set<ReturnReason>& ReturnPolicy::allowedReasons() const noexcept {
    return allowedReasons_;
}
const std::set<ItemCondition>& ReturnPolicy::restockableConditions() const noexcept {
    return restockableConditions_;
}
bool ReturnPolicy::returnEligible(const ProductCatalog& catalog, std::string_view sku) const {
    const auto item = catalog.productForSku(sku);
    return std::none_of(
        nonReturnableCategories_.begin(), nonReturnableCategories_.end(),
        [&catalog, &item](const auto& id) { return catalog.isInCategory(item.categoryId(), id); });
}
Timestamp ReturnPolicy::deadline(Timestamp deliveredAt) const {
    return detail::addNonnegative(deliveredAt, returnWindow_);
}
void ReturnPolicy::assertEligible(bool purchasedEligibility, Timestamp deliveredAt,
                                  ReturnReason reason, Timestamp now) const {
    detail::check<ReturnRuleException>(purchasedEligibility && allowedReasons_.contains(reason) &&
                                           now >= deliveredAt && now < deadline(deliveredAt),
                                       "Возврат недоступен по условиям покупки, причине или сроку");
}
bool ReturnPolicy::classifyRestockability(ItemCondition condition) const {
    detail::require(condition == ItemCondition::Unopened || condition == ItemCondition::Opened ||
                        condition == ItemCondition::Defective ||
                        condition == ItemCondition::Incomplete,
                    "Неизвестное состояние товара");
    return restockableConditions_.contains(condition);
}
} // namespace shop
