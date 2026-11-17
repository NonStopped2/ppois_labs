#include "shop/customers/Customer.hpp"
#include "shop/common/CustomerRestrictedException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"

namespace shop {

Customer::Customer(std::string_view id, ContactDetails contacts, Timestamp registeredAt)
    : id_(detail::text(id, "ID покупателя")), contacts_(std::move(contacts)),
      registeredAt_(registeredAt) {
    detail::require(registeredAt >= 0, "Некорректное время регистрации");
}
const std::string& Customer::id() const noexcept {
    return id_;
}
const ContactDetails& Customer::contacts() const noexcept {
    return contacts_;
}
Timestamp Customer::registeredAt() const noexcept {
    return registeredAt_;
}
CustomerStatus Customer::status() const noexcept {
    return status_;
}
const std::string& Customer::restrictionReason() const noexcept {
    return restrictionReason_;
}

void Customer::restrictPurchases(std::string_view reason) {
    auto explanation = detail::text(reason, "причина ограничения");
    detail::check<CustomerRestrictedException>(status_ == CustomerStatus::Active,
                                               "Покупки уже ограничены");
    restrictionReason_.swap(explanation);
    status_ = CustomerStatus::Restricted;
}
void Customer::restorePurchases() {
    detail::check<CustomerRestrictedException>(status_ == CustomerStatus::Restricted,
                                               "Покупатель не ограничен");
    restrictionReason_.clear();
    status_ = CustomerStatus::Active;
}
void Customer::assertMayCheckout() const {
    detail::check<CustomerRestrictedException>(status_ == CustomerStatus::Active,
                                               "Новые покупки ограничены: " + restrictionReason_);
}

} // namespace shop
