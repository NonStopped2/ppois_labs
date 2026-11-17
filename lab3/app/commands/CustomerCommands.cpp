#include "../ConsoleApplication.hpp"
#include "InputError.hpp"
#include "shop/application/Checkout.hpp"

namespace shop::ui {
void ConsoleApplication::customerCommands(std::string_view command, Arguments& args) {
    const auto action = args.take("действие");
    if (command == "customer") {
        requireOperator();
        const auto id = args.take("ID покупателя");
        if (action == "add") {
            const ContactDetails contacts{args.take("имя"), args.take("email"),
                                          args.take("телефон")};
            args.finish();
            state_.addCustomer(Customer(id, contacts, state_.now()));
        } else {
            auto customer = state_.customer(id);
            if (action == "restrict") {
                const auto reason = args.take("причина");
                args.finish();
                customer.restrictPurchases(reason);
            } else if (action == "restore") {
                args.finish();
                customer.restorePurchases();
            } else {
                throw InputError("Неизвестная команда покупателя");
            }
            state_.replaceCustomer(customer);
        }
    } else {
        auto book = state_.addresses(actor());
        if (action == "list") {
            args.finish();
            for (const auto& [label, address] : book.addresses()) {
                output_ << label << " | " << address.city() << ", " << address.street() << ' '
                        << address.building()
                        << (book.defaultAddressId() == label ? " [по умолчанию]\n" : "\n");
            }
            return;
        }
        const auto label = args.take("метка");
        if (action == "add" || action == "replace") {
            const Address address{"BY",
                                  args.take("индекс"),
                                  args.take("город"),
                                  args.take("улица"),
                                  args.take("дом"),
                                  args.take("квартира или пустая строка")};
            args.finish();
            if (action == "add") {
                book.add(label, address);
            } else {
                book.replace(label, address);
            }
        } else if (action == "remove") {
            args.finish();
            book.remove(label);
        } else if (action == "default") {
            args.finish();
            book.chooseDefault(label);
        } else {
            throw InputError("Неизвестная команда адресной книги");
        }
        state_.replaceAddresses(book);
    }
    output_ << "Готово\n";
}
void ConsoleApplication::cartCommands(std::string_view command, Arguments& args) {
    const auto customer = actor();
    const auto action = args.take("действие");
    if (command == "wish") {
        auto wishlist = state_.wishlist(customer);
        if (action == "show") {
            args.finish();
            for (const auto& id : wishlist.productIds()) {
                output_ << id << '\n';
            }
            return;
        }
        if (action == "add") {
            const auto id = args.take("продукт");
            args.finish();
            wishlist.addProduct(state_.catalog().product(id));
        } else if (action == "remove") {
            const auto id = args.take("продукт");
            args.finish();
            wishlist.removeProduct(id);
        } else if (action == "purge") {
            args.finish();
            output_ << "Убрано: " << wishlist.removeArchived(state_.catalog()) << '\n';
        } else if (action == "to-cart") {
            std::map<std::string, std::string> choices;
            while (!args.empty()) {
                const auto product = args.take("продукт");
                const auto sku = args.take("SKU");
                if (!choices.emplace(product, sku).second) {
                    throw InputError("Повторный выбор продукта");
                }
            }
            auto cart = state_.cart(customer);
            cart.mergeWishlist(wishlist, state_.catalog(), choices, state_.now(),
                               state_.pricingPolicy().maximumCartUnits());
            state_.replaceCart(cart);
            output_ << "Готово\n";
            return;
        } else {
            throw InputError("Неизвестная команда избранного");
        }
        state_.replaceWishlist(wishlist);
    } else {
        auto cart = state_.cart(customer);
        if (action == "show") {
            args.finish();
            showCart(cart);
            return;
        }
        if (action == "add") {
            const auto sku = args.take("SKU");
            const auto quantity = args.number("количество");
            args.finish();
            cart.addItem(state_.catalog(), sku, quantity, state_.now(),
                         state_.pricingPolicy().maximumCartUnits());
        } else if (action == "quantity") {
            const auto sku = args.take("SKU");
            const auto quantity = args.number("количество");
            args.finish();
            cart.changeQuantity(sku, quantity, state_.pricingPolicy().maximumCartUnits());
        } else if (action == "remove") {
            const auto sku = args.take("SKU");
            args.finish();
            cart.removeItem(sku);
        } else if (action == "coupon") {
            const auto code = args.take("код/-");
            args.finish();
            cart.setCoupon(code == "-" ? std::nullopt : std::optional(code));
        } else if (action == "adjust") {
            args.finish();
            const auto changes = cart.adjustToAvailableStock(state_.inventory());
            for (const auto& [sku, change] : changes) {
                output_ << sku << ": " << change.first << " -> " << change.second << '\n';
            }
        } else if (action == "prune") {
            args.finish();
            output_ << "Убрано: " << cart.removeUnavailable(state_.catalog()) << '\n';
        } else if (action == "switch") {
            const auto oldSku = args.take("старый SKU");
            const auto newSku = args.take("новый SKU");
            args.finish();
            cart.replaceVariant(state_.catalog(), oldSku, newSku);
        } else if (action == "clear") {
            args.finish();
            cart.clear();
        } else if (action == "wish") {
            const auto operation = args.take("ID операции");
            const auto skus = args.rest();
            Checkout(state_).moveCartItemsToWishlist(customer, skus, operation);
            output_ << "Готово\n";
            return;
        } else {
            throw InputError("Неизвестная команда корзины");
        }
        state_.replaceCart(cart);
    }
    output_ << "Готово\n";
}
} // namespace shop::ui
