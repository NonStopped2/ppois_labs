#include "shop/returns/ReturnLine.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/ReturnRuleException.hpp"

namespace shop {
ReturnLine::ReturnLine(const OrderLine& purchased, std::int64_t requestedQuantity,
                       std::int64_t alreadyReturnedOffset, ReturnReason reason)
    : sku_(purchased.sku()), requestedQuantity_(requestedQuantity),
      alreadyReturnedOffset_(alreadyReturnedOffset), reason_(reason) {
    detail::check<ReturnRuleException>(
        requestedQuantity > 0 && alreadyReturnedOffset >= 0 &&
            alreadyReturnedOffset <= purchased.quantity() &&
            requestedQuantity <= purchased.quantity() - alreadyReturnedOffset &&
            (reason == ReturnReason::ChangedMind || reason == ReturnReason::Defective ||
             reason == ReturnReason::WrongItem),
        "Некорректное возвратное количество, смещение или причина");
    maximumRefund_ =
        purchased.allocation().netForUnitRange(alreadyReturnedOffset, requestedQuantity);
}
const std::string& ReturnLine::sku() const noexcept {
    return sku_;
}
std::int64_t ReturnLine::requestedQuantity() const noexcept {
    return requestedQuantity_;
}
std::int64_t ReturnLine::alreadyReturnedOffset() const noexcept {
    return alreadyReturnedOffset_;
}
ReturnReason ReturnLine::reason() const noexcept {
    return reason_;
}
Money ReturnLine::maximumRefund() const noexcept {
    return maximumRefund_;
}
} // namespace shop
