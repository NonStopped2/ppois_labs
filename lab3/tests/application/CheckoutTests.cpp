#include "../StoreFixtures.hpp"
#include "shop/application/PreparedChange.hpp"
#include "shop/common/CartRuleException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/DuplicateOperationException.hpp"
#include "shop/common/EntityNotFoundException.hpp"
#include "shop/common/InsufficientStockException.hpp"
#include "shop/common/QuoteChangedException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

TEST_CASE(
    "Checkout atomically holds stock and coupon clears the cart and replays the same result") {
    auto state = test::store(true);
    Checkout checkout(state);
    const auto quote = checkout.preview("alice", test::address(), "standard");
    const auto before = state;
    REQUIRE(state.inventory().item("shirt-blue").reserved() == 0);
    REQUIRE(state.coupons().heldUses().empty());
    REQUIRE(state == before);
    const auto id = checkout.placeOrder("alice", quote, "place");
    REQUIRE(state.orders().order(id)->total() == Money(110));
    REQUIRE(state.cart("alice").lines().empty());
    REQUIRE_FALSE(state.cart("alice").couponCode());
    REQUIRE(state.inventory().item("shirt-blue").reserved() == 3);
    REQUIRE(state.coupons().heldUses().contains(id));
    REQUIRE(state.inventory().auditReservations(state.orders()).empty());
    const auto committed = state;
    REQUIRE(checkout.placeOrder("alice", quote, "place") == id);
    REQUIRE(state == committed);
    REQUIRE_THROWS_AS(checkout.placeOrder("bob", quote, "place"), DuplicateOperationException);
    REQUIRE_THROWS_AS(checkout.placeOrder("bob", quote, "foreign"), QuoteChangedException);
    REQUIRE(state == committed);
    REQUIRE(before.cart("alice").totalUnits() == 3);
}

TEST_CASE(
    "Checkout rejects changed quotes exhausted stock and expired offers without partial effects") {
    auto state = test::store();
    Checkout checkout(state);
    auto quote = checkout.preview("alice", test::address(), "standard");
    state.changePrice("shirt-blue", Money(50));
    const auto changed = state;
    REQUIRE_THROWS_AS(checkout.placeOrder("alice", quote, "place"), QuoteChangedException);
    REQUIRE(state == changed);
    quote = checkout.preview("alice", test::address(), "standard");
    state.advanceClock(10);
    const auto expired = state;
    REQUIRE_THROWS_AS(checkout.placeOrder("alice", quote, "place"), QuoteChangedException);
    REQUIRE(state == expired);
    auto insufficient = state.cart("alice");
    insufficient.changeQuantity("shirt-blue", 21);
    state.replaceCart(insufficient);
    const auto noStock = state;
    REQUIRE_THROWS_AS(checkout.preview("alice", test::address(), "standard"),
                      InsufficientStockException);
    REQUIRE(state == noStock);
    REQUIRE_THROWS_AS(checkout.preview("bob", test::address(), "standard"), CartRuleException);
}

TEST_CASE(
    "Expiry closes unpaid order reservation and coupon together and leaves paid orders alone") {
    auto state = test::store(true);
    const auto id = test::place(state);
    state.advanceClock(30);
    REQUIRE(Checkout(state).expireUnpaidOrders() == std::vector<std::string>{id});
    REQUIRE(state.orders().order(id)->status() == OrderStatus::Expired);
    REQUIRE(state.inventory().item("shirt-blue").reserved() == 0);
    REQUIRE(state.coupons().heldUses().empty());
    REQUIRE(state.inventory().auditReservations(state.orders()).empty());
    REQUIRE(Checkout(state).expireUnpaidOrders().empty());
    auto paid = test::store(true);
    const auto paidId = test::place(paid);
    test::pay(paid, paidId);
    paid.advanceClock(1000);
    REQUIRE(Checkout(paid).expireUnpaidOrders().empty());
    REQUIRE(paid.inventory().item("shirt-blue").reserved() == 3);
}

TEST_CASE("Moving cart selections folds variants into wishes and rolls back a late unknown SKU") {
    auto state = test::store();
    Checkout checkout(state);
    auto catalog = state.catalog();
    catalog.addVariant("shirt",
                       ProductVariant("shirt-red", "red", {{"color", "red"}}, 1, {1, 1, 1}));
    state.replaceCatalog(catalog);
    auto cart = state.cart("alice");
    cart.addItem(catalog, "shirt-red", 1, state.now());
    state.replaceCart(cart);
    const auto before = state;
    REQUIRE_THROWS_AS(checkout.moveCartItemsToWishlist("alice", {"shirt-blue", "unknown"}, "move"),
                      EntityNotFoundException);
    REQUIRE(state == before);
    REQUIRE_THROWS_AS(
        checkout.moveCartItemsToWishlist("alice", {"shirt-blue", "shirt-blue"}, "move"),
        DuplicateEntityException);
    checkout.moveCartItemsToWishlist("alice", {"shirt-red", "shirt-blue"}, "move");
    REQUIRE(state.wishlist("alice").productIds() == std::set<std::string>{"shirt"});
    REQUIRE(state.cart("alice").lines().empty());
    const auto committed = state;
    checkout.moveCartItemsToWishlist("alice", {"shirt-blue", "shirt-red"}, "move");
    REQUIRE(state == committed);
}

TEST_CASE(
    "Prepared changes are discarded unless committed and commit cannot toggle old values back") {
    auto state = test::store();
    const auto before = state;
    {
        PreparedChange change(state);
        change.carts().at("alice").clear();
    }
    REQUIRE(state == before);
    {
        PreparedChange change(state);
        change.carts().at("alice").clear();
        change.commit();
        change.commit();
    }
    REQUIRE(state.cart("alice").lines().empty());
    REQUIRE(before.cart("alice").totalUnits() == 3);
}
