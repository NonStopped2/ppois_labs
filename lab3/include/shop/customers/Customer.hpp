#pragma once

#include "shop/common/ContactDetails.hpp"
#include "shop/common/TimeWindow.hpp"
#include <string>
#include <string_view>

namespace shop {

enum class CustomerStatus { Active, Restricted };

/** Customer.
 * Хранит контакты и ограничения покупателя. Ограничение блокирует новые покупки, уже
 * оформленные заказы можно сопровождать.
 */
class Customer {
  public:
    Customer(std::string_view id, ContactDetails contacts, Timestamp registeredAt);
    const std::string& id() const noexcept;
    const ContactDetails& contacts() const noexcept;
    Timestamp registeredAt() const noexcept;
    CustomerStatus status() const noexcept;
    const std::string& restrictionReason() const noexcept;
    void restrictPurchases(std::string_view reason);
    void restorePurchases();
    void assertMayCheckout() const;
    bool operator==(const Customer&) const = default;

  private:
    std::string id_;
    ContactDetails contacts_;
    CustomerStatus status_ = CustomerStatus::Active;
    Timestamp registeredAt_;
    std::string restrictionReason_;
};

} // namespace shop
