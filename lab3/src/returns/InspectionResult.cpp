#include "shop/returns/InspectionResult.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/ReturnRuleException.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
InspectionResult::InspectionResult(std::string_view sku, std::int64_t receivedQuantity,
                                   std::int64_t acceptedQuantity, std::int64_t restockQuantity,
                                   ItemCondition condition)
    : sku_(detail::text(sku, "осмотренный SKU")), receivedQuantity_(receivedQuantity),
      acceptedQuantity_(acceptedQuantity), restockQuantity_(restockQuantity),
      condition_(condition) {
    assertConsistent();
}
const std::string& InspectionResult::sku() const noexcept {
    return sku_;
}
std::int64_t InspectionResult::receivedQuantity() const noexcept {
    return receivedQuantity_;
}
std::int64_t InspectionResult::acceptedQuantity() const noexcept {
    return acceptedQuantity_;
}
std::int64_t InspectionResult::restockQuantity() const noexcept {
    return restockQuantity_;
}
ItemCondition InspectionResult::condition() const noexcept {
    return condition_;
}
void InspectionResult::assertConsistent() const {
    detail::check<ReturnRuleException>(
        restockQuantity_ >= 0 && acceptedQuantity_ >= restockQuantity_ &&
            receivedQuantity_ >= acceptedQuantity_ &&
            (condition_ == ItemCondition::Unopened || condition_ == ItemCondition::Opened ||
             condition_ == ItemCondition::Defective || condition_ == ItemCondition::Incomplete),
        "Нарушено restock <= accepted <= received или неизвестно состояние товара");
    detail::check<ReturnRuleException>(restockQuantity_ == 0 ||
                                           condition_ == ItemCondition::Unopened ||
                                           condition_ == ItemCondition::Opened,
                                       "Брак и некомплект не возвращаются в доступный остаток");
}
} // namespace shop
