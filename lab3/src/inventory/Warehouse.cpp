#include "shop/inventory/Warehouse.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
Warehouse::Warehouse(std::string_view id, std::string_view name, Address address,
                     bool acceptingReceipts)
    : id_(detail::text(id, "склад")), name_(detail::text(name, "название склада")),
      address_(std::move(address)), acceptingReceipts_(acceptingReceipts) {}
const std::string& Warehouse::id() const noexcept {
    return id_;
}
const std::string& Warehouse::name() const noexcept {
    return name_;
}
const Address& Warehouse::address() const noexcept {
    return address_;
}
bool Warehouse::acceptingReceipts() const noexcept {
    return acceptingReceipts_;
}
void Warehouse::setAcceptingReceipts(bool enabled) noexcept {
    acceptingReceipts_ = enabled;
}
void Warehouse::assertMayReceive() const {
    detail::require(acceptingReceipts_, "Склад не принимает поставки");
}
} // namespace shop
