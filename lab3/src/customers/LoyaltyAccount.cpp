#include "shop/customers/LoyaltyAccount.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/DuplicateOperationException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"

namespace shop {

LoyaltyAccount::LoyaltyAccount(std::string_view customerId)
    : customerId_(detail::text(customerId, "покупатель")) {}
const std::string& LoyaltyAccount::customerId() const noexcept {
    return customerId_;
}
Money LoyaltyAccount::qualifyingSpend() const noexcept {
    return qualifyingSpend_;
}
const std::map<std::string, Money>& LoyaltyAccount::netDeliveredPurchases() const noexcept {
    return netDeliveredPurchases_;
}

bool LoyaltyAccount::replay(std::string_view operationId, const Operation& operation) const {
    const auto found = processedOperations_.find(std::string(operationId));
    if (found == processedOperations_.end()) {
        return false;
    }
    detail::check<DuplicateOperationException>(
        found->second == operation, "ID операции лояльности использован с другими данными");
    return true;
}

void LoyaltyAccount::record(std::string_view orderId, Money amount, std::string_view operationId,
                            bool returning) {
    const auto order = detail::text(orderId, "заказ");
    const auto key = detail::text(operationId, "операция");
    detail::require(amount >= Money{}, "Товарный оборот не может быть отрицательным");
    const Operation operation{order, amount, returning};
    if (replay(key, operation)) {
        return;
    }
    auto purchases = netDeliveredPurchases_;
    auto operations = processedOperations_;
    Money resulting;
    if (returning) {
        const auto balance = detail::get(purchases, order);
        detail::require(amount <= balance, "Возврат превышает чистый оборот заказа");
        purchases.at(order) = balance.checkedSubtract(amount);
        resulting = qualifyingSpend_.checkedSubtract(amount);
    } else {
        detail::check<DuplicateEntityException>(purchases.emplace(order, amount).second,
                                                "Получение заказа уже учтено");
        resulting = qualifyingSpend_.checkedAdd(amount);
    }
    operations.emplace(key, operation);
    netDeliveredPurchases_.swap(purchases);
    processedOperations_.swap(operations);
    qualifyingSpend_ = resulting;
}
void LoyaltyAccount::recordDelivery(std::string_view orderId, Money goodsNet,
                                    std::string_view operationId) {
    record(orderId, goodsNet, operationId, false);
}
void LoyaltyAccount::recordReturn(std::string_view orderId, Money returned,
                                  std::string_view operationId) {
    record(orderId, returned, operationId, true);
}

} // namespace shop
