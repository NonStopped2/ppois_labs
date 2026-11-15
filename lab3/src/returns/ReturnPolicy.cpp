#include "shop/returns/ReturnPolicy.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/ReturnRuleException.hpp"
#include "shop/common/Validation.hpp"
#include "shop/orders/Order.hpp"
#include "shop/returns/ReturnInspection.hpp"
#include "shop/returns/ReturnRequest.hpp"
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
Money ReturnPolicy::calculateRefund(
    const Order& order, const ReturnRequest& request, const ReturnInspection& inspection,
    const std::map<std::string, std::int64_t>& completedQuantities) const {
    detail::check<ReturnRuleException>(order.id() == request.orderId() &&
                                           request.customerId() == order.customerId() &&
                                           inspection.requestId() == request.id() &&
                                           inspection.finalized() && *this == order.returnPolicy(),
                                       "Возвратные документы не согласованы с покупкой");
    Money result;
    for (const auto& entry : request.lines()) {
        const auto purchased = order.line(entry.sku());
        const auto prior = completedQuantities.find(entry.sku());
        const auto offset = prior == completedQuantities.end() ? 0 : prior->second;
        const auto& inspected = detail::get(inspection.results(), entry.sku());
        detail::check<ReturnRuleException>(offset == entry.alreadyReturnedOffset() &&
                                               inspected.acceptedQuantity() <=
                                                   entry.requestedQuantity(),
                                           "Количество изменилось после открытия заявки");
        result = result.checkedAdd(
            purchased.allocation().netForUnitRange(offset, inspected.acceptedQuantity()));
    }
    return result;
}
} // namespace shop
