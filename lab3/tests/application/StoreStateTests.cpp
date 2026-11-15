#include "../PurchaseFixtures.hpp"
#include "../StoreFixtures.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/EntityNotFoundException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

TEST_CASE("Store state owns customer records and rejects orphaning catalog changes") {
    auto state = test::store(true);
    REQUIRE(state.now() == 10);
    REQUIRE(state.prices().price("shirt-blue").unitPrice() == Money(40));
    REQUIRE(state.promotions().size() == 1);
    REQUIRE(state.operations().empty());
    REQUIRE(state.fulfillments().empty());
    REQUIRE(state.pricingPolicy().maximumCartUnits() == 1000);
    REQUIRE(state.deliveryPolicy().options().size() == 3);
    auto addresses = state.addresses("alice");
    addresses.add("home", test::address());
    state.replaceAddresses(addresses);
    REQUIRE(state.addresses("alice").defaultAddressId() == "home");
    auto wishlist = state.wishlist("alice");
    wishlist.addProduct(state.catalog().product("shirt"));
    state.replaceWishlist(wishlist);
    REQUIRE(state.wishlist("alice").productIds().contains("shirt"));
    auto customer = state.customer("alice");
    customer.restrictPurchases("Проверка");
    state.replaceCustomer(customer);
    REQUIRE(state.customer("alice").status() == CustomerStatus::Restricted);
    const auto before = state;
    REQUIRE_THROWS_AS(state.addCustomer(customer), DuplicateEntityException);
    REQUIRE_THROWS_AS(state.replaceCustomer(Customer("alice", customer.contacts(), 1)),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(state.replaceCatalog(ProductCatalog{}), EntityNotFoundException);
    REQUIRE_THROWS_AS(state.replaceCart(Cart("other", "alice")), InvalidDomainDataException);
    REQUIRE_THROWS_AS(state.replaceAddresses(AddressBook("unknown")), EntityNotFoundException);
    REQUIRE_THROWS_AS(state.replaceWishlist(Wishlist("unknown")), EntityNotFoundException);
    REQUIRE_THROWS_AS(state.addCustomer(Customer("future", customer.contacts(), 11)),
                      InvalidDomainDataException);
    REQUIRE(state == before);
    state.disableCoupon("SAVE");
    REQUIRE_FALSE(state.coupons().coupon("SAVE").enabled());
    REQUIRE(before.coupons().coupon("SAVE").enabled());
    REQUIRE(state.nextId("test") == "test-1");
    REQUIRE(state.nextId("test") == "test-2");
    REQUIRE_FALSE(state == before);
}

TEST_CASE("Inventory audit describes missing wrong and mismatched order reservations") {
    auto state = test::store();
    const auto id = test::place(state);
    auto inventory = state.inventory();
    auto orders = state.orders();
    REQUIRE(inventory.auditReservations(orders).empty());
    REQUIRE_FALSE(inventory.auditReservations(OrderBook{}).empty());
    OrderBook missing;
    missing.add(test::order("missing"));
    REQUIRE(inventory.auditReservations(missing).size() == 2);
    inventory.confirm(orders.order(id)->reservationId(), state.now());
    REQUIRE_FALSE(inventory.auditReservations(orders).empty());
    auto different = *orders.order(id);
    different.extendPaymentDeadline(state.now(), 10, "extend");
    orders.replace(different);
    REQUIRE(inventory.auditReservations(orders).size() == 2);
}
