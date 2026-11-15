#include "../Fixtures.hpp"
#include "shop/cart/Cart.hpp"
#include "shop/common/CartRuleException.hpp"
#include "shop/common/EntityNotFoundException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

TEST_CASE("Cart merges quantities without holding stock and rejects invalid changes atomically") {
    auto catalog = test::catalog();
    Cart cart("cart", "alice");
    REQUIRE(cart.id() == "cart");
    REQUIRE(cart.customerId() == "alice");
    REQUIRE(cart.revision() == 0);
    REQUIRE_FALSE(cart.couponCode());
    REQUIRE_THROWS_AS(cart.assertCheckoutReady("alice", 100), CartRuleException);
    cart.addItem(catalog, "shirt-blue", 2, 10, 3);
    cart.addItem(catalog, "shirt-blue", 1, 11, 3);
    REQUIRE(cart.totalUnits() == 3);
    REQUIRE(cart.lines().at("shirt-blue").sku() == "shirt-blue");
    REQUIRE(cart.lines().at("shirt-blue").addedAt() == 10);
    const auto before = cart;
    REQUIRE_THROWS_AS(cart.addItem(catalog, "shirt-blue", 1, 12, 3), CartRuleException);
    REQUIRE_THROWS_AS(cart.changeQuantity("shirt-blue", 4, 3), CartRuleException);
    REQUIRE_THROWS_AS(cart.changeQuantity("shirt-blue", 0, 3), CartRuleException);
    REQUIRE_THROWS_AS(cart.assertCheckoutReady("bob", 3), CartRuleException);
    REQUIRE_THROWS_AS(cart.addItem(catalog, "shirt-blue", 1, 9), InvalidDomainDataException);
    REQUIRE_THROWS_AS(cart.removeItem("missing"), EntityNotFoundException);
    REQUIRE(cart == before);
    cart.changeQuantity("shirt-blue", 2);
    cart.assertCheckoutReady("alice", 2);
    cart.setCoupon(" save ");
    REQUIRE(cart.couponCode() == "SAVE");
    cart.setCoupon(std::nullopt);
    REQUIRE_FALSE(cart.couponCode());
    cart.removeItem("shirt-blue");
    REQUIRE(cart.lines().empty());
    cart.addItem(catalog, "shirt-blue", 1, 20);
    cart.setCoupon("X");
    cart.clear();
    REQUIRE(cart.lines().empty());
    REQUIRE_FALSE(cart.couponCode());
    REQUIRE_THROWS_AS(CartLine("s", 0, 0), InvalidDomainDataException);
}

TEST_CASE("Cart variant replacement preserves total quantity and earliest addition time") {
    auto catalog = test::catalog();
    catalog.addVariant("shirt",
                       ProductVariant("shirt-red", "red", {{"color", "red"}}, 1, {1, 1, 1}));
    Cart cart("cart", "alice");
    cart.addItem(catalog, "shirt-blue", 2, 10);
    cart.addItem(catalog, "shirt-red", 1, 20);
    cart.replaceVariant(catalog, "shirt-blue", "shirt-red");
    REQUIRE(cart.totalUnits() == 3);
    REQUIRE(cart.lines().size() == 1);
    REQUIRE(cart.lines().at("shirt-red").addedAt() == 10);
    REQUIRE_THROWS_AS(cart.replaceVariant(catalog, "shirt-red", "shirt-red"), CartRuleException);
    cart.replaceVariant(catalog, "shirt-red", "shirt-blue");
    REQUIRE(cart.lines().at("shirt-blue").quantity() == 3);
    REQUIRE(cart.removeUnavailable(catalog) == 0);
    catalog.setVariantAvailability("shirt-blue", false);
    REQUIRE_THROWS_AS(cart.addItem(catalog, "shirt-blue", 1, 30), CartRuleException);
    REQUIRE(cart.removeUnavailable(catalog) == 1);
}

TEST_CASE("Wishlist merging and explicit stock adjustment are all-or-nothing") {
    auto catalog = test::catalog();
    Wishlist wishes("alice");
    wishes.addProduct(catalog.product("shirt"));
    Cart cart("cart", "alice");
    cart.mergeWishlist(wishes, catalog, {}, 10);
    cart.mergeWishlist(wishes, catalog, {}, 11);
    REQUIRE(cart.totalUnits() == 2);
    const auto before = cart;
    REQUIRE_THROWS_AS(cart.mergeWishlist(wishes, catalog, {}, 12, 2), CartRuleException);
    REQUIRE_THROWS_AS(cart.mergeWishlist(Wishlist("bob"), catalog, {}, 12), CartRuleException);
    REQUIRE(cart == before);
    Inventory stock(Warehouse("w", "Склад", test::address()));
    stock.addStockItem(catalog, "shirt-blue", 0);
    stock.applyStockCount("shirt-blue", 1);
    REQUIRE(cart.adjustToAvailableStock(stock).at("shirt-blue") ==
            std::make_pair(std::int64_t{2}, std::int64_t{1}));
    REQUIRE(cart.adjustToAvailableStock(stock).empty());
    REQUIRE(stock.item("shirt-blue").reserved() == 0);
    stock.applyStockCount("shirt-blue", 0);
    REQUIRE(cart.adjustToAvailableStock(stock).at("shirt-blue").second == 0);
    REQUIRE(cart.lines().empty());
}
