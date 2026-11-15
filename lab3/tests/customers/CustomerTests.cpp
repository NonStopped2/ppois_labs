#include "../Fixtures.hpp"
#include "shop/common/CatalogRuleException.hpp"
#include "shop/common/CustomerRestrictedException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/EntityNotFoundException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/customers/AddressBook.hpp"
#include "shop/customers/Wishlist.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

TEST_CASE("Customer restriction preserves data and is separate from old orders") {
    auto customer = test::customer();
    REQUIRE(customer.id() == "alice");
    REQUIRE(customer.contacts().fullName() == "Анна");
    REQUIRE(customer.registeredAt() == 0);
    REQUIRE(customer.restrictionReason().empty());
    REQUIRE(customer.status() == CustomerStatus::Active);
    REQUIRE_NOTHROW(customer.assertMayCheckout());
    const auto before = customer;
    REQUIRE_THROWS_AS(customer.restrictPurchases(" "), InvalidDomainDataException);
    REQUIRE_THROWS_AS(customer.restorePurchases(), CustomerRestrictedException);
    REQUIRE(customer == before);
    customer.restrictPurchases("проверка");
    REQUIRE(customer.restrictionReason() == "проверка");
    REQUIRE_THROWS_AS(customer.assertMayCheckout(), CustomerRestrictedException);
    REQUIRE_THROWS_AS(customer.restrictPurchases("другая"), CustomerRestrictedException);
    customer.restorePurchases();
    REQUIRE(customer == before);
    REQUIRE_THROWS_AS(Customer("x", customer.contacts(), -1), InvalidDomainDataException);
}

TEST_CASE("Address book selects a valid default and owns independent snapshots") {
    AddressBook book("alice");
    REQUIRE(book.customerId() == "alice");
    REQUIRE_FALSE(book.defaultAddressId());
    book.add("home", test::address());
    book.add("work", Address("BY", "220002", "Минск", "Полевая", "2", "3"));
    REQUIRE(book.addresses().size() == 2);
    REQUIRE(book.defaultAddressId() == "home");
    const auto saved = book.address("home");
    book.chooseDefault("work");
    REQUIRE(book.defaultAddressId() == "work");
    const auto before = book;
    REQUIRE_THROWS_AS(book.add("home", saved), DuplicateEntityException);
    REQUIRE_THROWS_AS(book.remove("missing"), EntityNotFoundException);
    REQUIRE_THROWS_AS(book.chooseDefault("missing"), EntityNotFoundException);
    REQUIRE_THROWS_AS(book.replace("missing", saved), EntityNotFoundException);
    REQUIRE(book == before);
    book.replace("home", book.address("work"));
    REQUIRE(saved == test::address());
    book.remove("home");
    REQUIRE(book.defaultAddressId() == "work");
    book.add("a", saved);
    book.remove("work");
    REQUIRE(book.defaultAddressId() == "a");
    book.remove("a");
    REQUIRE_FALSE(book.defaultAddressId());
}

TEST_CASE("Wishlist resolves variants explicitly and purges archived products") {
    auto catalog = test::catalog();
    Wishlist wishlist("alice");
    REQUIRE(wishlist.customerId() == "alice");
    wishlist.addProduct(catalog.product("shirt"));
    REQUIRE(wishlist.productIds().size() == 1);
    REQUIRE(wishlist.createCartSelection(catalog) == std::vector<std::string>{"shirt-blue"});
    REQUIRE_THROWS_AS(wishlist.addProduct(catalog.product("shirt")), DuplicateEntityException);
    REQUIRE_THROWS_AS(wishlist.addProduct(test::product()), CatalogRuleException);
    REQUIRE_THROWS_AS(wishlist.removeProduct("missing"), EntityNotFoundException);
    REQUIRE_THROWS_AS(wishlist.createCartSelection(catalog, {{"missing", "sku"}}),
                      CatalogRuleException);
    catalog.addVariant("shirt",
                       ProductVariant("shirt-red", "red", {{"color", "red"}}, 1, {1, 1, 1}));
    REQUIRE_THROWS_AS(wishlist.createCartSelection(catalog), CatalogRuleException);
    REQUIRE_THROWS_AS(wishlist.createCartSelection(catalog, {{"shirt", "missing"}}),
                      CatalogRuleException);
    REQUIRE(wishlist.createCartSelection(catalog, {{"shirt", "shirt-red"}}).front() == "shirt-red");
    REQUIRE(wishlist.removeArchived(catalog) == 0);
    const auto copy = wishlist;
    wishlist.removeProduct("shirt");
    REQUIRE(copy.productIds().size() == 1);
    wishlist = copy;
    catalog.archiveProduct("shirt");
    REQUIRE(wishlist.createCartSelection(catalog).empty());
    REQUIRE(wishlist.removeArchived(catalog) == 1);
    REQUIRE(wishlist.productIds().empty());
}
