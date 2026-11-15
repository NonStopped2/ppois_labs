#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
using namespace snapshot;
void SnapshotCodec::readCustomers(StoreState& state, const Json& data) {
    for (const auto& saved : array(data)) {
        Customer customer(string(saved.at("id")), contacts(saved.at("contacts")),
                          integer(saved.at("registered_at")));
        const auto status = enumeration(saved.at("status"), CustomerStatus::Restricted);
        const auto reason = string(saved.at("restriction_reason"));
        if (status == CustomerStatus::Restricted) {
            customer.restrictPurchases(reason);
        } else {
            require(reason.empty(), "Причина ограничения активного покупателя");
        }
        state.addCustomer(customer);
        const auto& id = customer.id();
        AddressBook addresses(id);
        for (const auto& [label, entry] : object(saved.at("addresses")).items()) {
            addresses.add(label, address(entry));
        }
        const auto selected = optionalString(saved.at("default_address"));
        require(selected.has_value() != addresses.addresses().empty(),
                "Адрес по умолчанию не согласован с книгой");
        if (selected) {
            addresses.chooseDefault(*selected);
        }
        state.addressBooks_.at(id) = std::move(addresses);
        Wishlist wishlist(id);
        wishlist.productIds_ = stringSet(saved.at("wishlist"));
        state.wishlists_.at(id) = std::move(wishlist);
        const auto& cartData = object(saved.at("cart"));
        Cart cart(string(cartData.at("id")), id);
        for (const auto& line : array(cartData.at("lines"))) {
            const CartLine entry(string(line.at("sku")), integer(line.at("quantity")),
                                 integer(line.at("added_at")));
            insertUnique(cart.lines_, entry.sku(), entry);
        }
        cart.couponCode_ = optionalString(cartData.at("coupon"));
        cart.revision_ = integer(cartData.at("revision"));
        require(cart.revision_ >= 0, "Отрицательная ревизия корзины");
        state.carts_.at(id) = std::move(cart);
        const auto& loyaltyData = object(saved.at("loyalty"));
        LoyaltyAccount account(id);
        for (const auto& [orderId, amount] : object(loyaltyData.at("net_purchases")).items()) {
            const Money money(integer(amount));
            require(money >= Money{}, "Отрицательный товарный оборот заказа");
            account.netDeliveredPurchases_.emplace(orderId, money);
            account.qualifyingSpend_ = account.qualifyingSpend_.checkedAdd(money);
        }
        require(account.qualifyingSpend_ == Money(integer(loyaltyData.at("qualifying_spend"))),
                "Кеш оборота лояльности не совпадает с покупками");
        for (const auto& entry : array(loyaltyData.at("operations"))) {
            const Money amount(integer(entry.at("amount")));
            require(amount >= Money{}, "Отрицательное движение лояльности");
            insertUnique(account.processedOperations_, string(entry.at("id")),
                         LoyaltyAccount::Operation{string(entry.at("order_id")), amount,
                                                   boolean(entry.at("returning"))});
        }
        state.loyaltyAccounts_.at(id) = std::move(account);
    }
}
Json SnapshotCodec::writeCustomers(const StoreState& state) {
    Json result = Json::array();
    for (const auto& [id, customer] : state.customers_) {
        const auto& addresses = state.addressBooks_.at(id);
        const auto& wishlist = state.wishlists_.at(id);
        const auto& cart = state.carts_.at(id);
        const auto& loyalty = state.loyaltyAccounts_.at(id);
        Json encodedAddresses = Json::object();
        for (const auto& [label, entry] : addresses.addresses()) {
            encodedAddresses[label] = value(entry);
        }
        Json purchases = Json::object();
        for (const auto& [orderId, amount] : loyalty.netDeliveredPurchases()) {
            purchases[orderId] = amount.kopecks();
        }
        result.push_back(
            {{"id", id},
             {"contacts", value(customer.contacts())},
             {"registered_at", customer.registeredAt()},
             {"status", enumValue(customer.status())},
             {"restriction_reason", customer.restrictionReason()},
             {"addresses", encodedAddresses},
             {"default_address", nullable(addresses.defaultAddressId())},
             {"wishlist", wishlist.productIds()},
             {"cart",
              {{"id", cart.id()},
               {"revision", cart.revision()},
               {"coupon", nullable(cart.couponCode())},
               {"lines", arrayOf(cart.lines(),
                                 [](const auto& pair) {
                                     const auto& line = pair.second;
                                     return Json{{"sku", line.sku()},
                                                 {"quantity", line.quantity()},
                                                 {"added_at", line.addedAt()}};
                                 })}}},
             {"loyalty",
              {{"net_purchases", purchases},
               {"qualifying_spend", loyalty.qualifyingSpend().kopecks()},
               {"operations", arrayOf(loyalty.processedOperations_, [](const auto& pair) {
                    const auto& [orderId, amount, returning] = pair.second;
                    return Json{{"id", pair.first},
                                {"order_id", orderId},
                                {"amount", amount.kopecks()},
                                {"returning", returning}};
                })}}}});
    }
    return result;
}
} // namespace shop
