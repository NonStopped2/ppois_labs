#include "post/sorting/SortingRule.hpp"
#include "post/detail/PostalPrefixes.hpp"
#include <set>

namespace post {
// Набор продуктов и покрытие проверяются без зависимости от будущего PostalBranch.
SortingRule::SortingRule(const std::string& id, std::vector<std::string> destinationPrefixes,
                         const std::string& nextBranchId, std::int64_t priority,
                         std::vector<ShipmentKind> allowedShipmentKinds)
    : id_(detail::identifier(id)),
      destinationPrefixes_(detail::postalPrefixes(std::move(destinationPrefixes))),
      nextBranchId_(detail::identifier(nextBranchId)), priority_(priority),
      allowedShipmentKinds_(std::move(allowedShipmentKinds)) {
    if (priority_ < 0 || allowedShipmentKinds_.empty()) {
        throw std::invalid_argument("Invalid sorting rule");
    }
    std::set<ShipmentKind> kinds;
    std::for_each(allowedShipmentKinds_.begin(), allowedShipmentKinds_.end(),
                  [&kinds](const auto kind) {
                      if ((kind != ShipmentKind::Letter && kind != ShipmentKind::Parcel) ||
                          !kinds.insert(kind).second) {
                          throw std::invalid_argument("Invalid sorting products");
                      }
                  });
}
// Идентификатор должен быть уникален внутри плана.
const std::string& SortingRule::id() const noexcept {
    return id_;
}
// Префиксы сохраняют ведущие нули.
const std::vector<std::string>& SortingRule::destinationPrefixes() const noexcept {
    return destinationPrefixes_;
}
// Направление — идентификатор следующего, не обязательно последнего отделения.
const std::string& SortingRule::nextBranchId() const noexcept {
    return nextBranchId_;
}
// Равные приоритеты разрешены лишь без неоднозначного направления.
std::int64_t SortingRule::priority() const noexcept {
    return priority_;
}
// Ограничение продукта участвует и в поиске конфликтов.
const std::vector<ShipmentKind>& SortingRule::allowedShipmentKinds() const noexcept {
    return allowedShipmentKinds_;
}
// При возврате адрес назначения — исходный отправитель, сами снимки клиентов не переставляются.
bool SortingRule::matches(const Shipment& shipment) const {
    if (std::find(allowedShipmentKinds_.begin(), allowedShipmentKinds_.end(), shipment.kind()) ==
        allowedShipmentKinds_.end()) {
        return false;
    }
    const auto& address = shipment.status() == ShipmentStatus::Returning
                              ? shipment.sender().address()
                              : shipment.recipient().address();
    return std::any_of(
        destinationPrefixes_.begin(), destinationPrefixes_.end(),
        [&address](const auto& prefix) { return address.postalCode().starts_with(prefix); });
}
// Одно направление при одинаковом приоритете не создаёт неоднозначности.
bool SortingRule::conflictsWith(const SortingRule& other) const {
    const bool commonKind = std::any_of(
        allowedShipmentKinds_.begin(), allowedShipmentKinds_.end(), [&other](const auto kind) {
            return std::find(other.allowedShipmentKinds_.begin(), other.allowedShipmentKinds_.end(),
                             kind) != other.allowedShipmentKinds_.end();
        });
    return priority_ == other.priority_ && nextBranchId_ != other.nextBranchId_ && commonKind &&
           detail::prefixesOverlap(destinationPrefixes_, other.destinationPrefixes_);
}
} // namespace post
