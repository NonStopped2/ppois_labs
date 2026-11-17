#include "../Fixtures.hpp"
#include "shop/common/CatalogRuleException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/DuplicateOperationException.hpp"
#include "shop/common/EntityNotFoundException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/customers/LoyaltyPolicy.hpp"
#include "shop/pricing/DiscountAllocation.hpp"
#include "shop/pricing/PriceList.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace shop;

TEST_CASE("Partial-return allocations preserve every kopeck under all small partitions") {
    for (std::int64_t units = 1; units <= 10; ++units) {
        for (std::int64_t gross = 0; gross <= 30; ++gross) {
            for (std::int64_t discount = 0; discount <= gross; ++discount) {
                const DiscountAllocation allocation("sku", units, Money(gross), Money(discount));
                for (std::int64_t split = 0; split <= units; ++split) {
                    REQUIRE(allocation.netForUnitRange(0, split).checkedAdd(
                                allocation.netForUnitRange(split, units - split)) ==
                            Money(gross - discount));
                }
            }
        }
    }
    const DiscountAllocation allocation("sku", 3, Money(120), Money(20));
    REQUIRE(allocation.sku() == "sku");
    REQUIRE(allocation.quantity() == 3);
    REQUIRE(allocation.grossTotal() == Money(120));
    REQUIRE(allocation.discountTotal() == Money(20));
    REQUIRE(allocation.netForUnitRange(0, 1) == Money(34));
    REQUIRE(allocation.netForUnitRange(1, 1) == Money(33));
    REQUIRE(allocation.netForUnitRange(2, 1) == Money(33));
    REQUIRE_THROWS_AS(allocation.netForUnitRange(-1, 1), InvalidDomainDataException);
    REQUIRE_THROWS_AS(allocation.netForUnitRange(2, 2), InvalidDomainDataException);
    REQUIRE_THROWS_AS(allocation.netForUnitRange(4, 0), InvalidDomainDataException);
    REQUIRE_THROWS_AS(DiscountAllocation("sku", 0, Money(1), Money{}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(DiscountAllocation("sku", 1, Money(1), Money(2)), InvalidDomainDataException);
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    REQUIRE(
        DiscountAllocation("sku", maximum, Money(maximum), Money{}).netForUnitRange(0, maximum) ==
        Money(maximum));
}

TEST_CASE("Loyalty turnover replays safely and rejects excessive returns without partial changes") {
    LoyaltyAccount account("alice");
    REQUIRE(account.customerId() == "alice");
    account.recordDelivery("order", Money(1000), "delivery");
    account.recordDelivery("order", Money(1000), "delivery");
    REQUIRE(account.qualifyingSpend() == Money(1000));
    REQUIRE(account.netDeliveredPurchases().at("order") == Money(1000));
    const auto before = account;
    REQUIRE_THROWS_AS(account.recordDelivery("order", Money(999), "delivery"),
                      DuplicateOperationException);
    REQUIRE_THROWS_AS(account.recordReturn("order", Money(1001), "return"),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(account.recordReturn("missing", Money(1), "return"), EntityNotFoundException);
    REQUIRE_THROWS_AS(account.recordDelivery("order", Money(1000), "otherDelivery"),
                      DuplicateEntityException);
    REQUIRE(account == before);
    account.recordReturn("order", Money(400), "return");
    account.recordReturn("order", Money(400), "return");
    REQUIRE(account.qualifyingSpend() == Money(600));
    account.recordReturn("order", Money(600), "return2");
    REQUIRE(account.qualifyingSpend() == Money{});
    REQUIRE(before.qualifyingSpend() == Money(1000));
    account.recordDelivery("free", Money{}, "free-delivery");
}

TEST_CASE("Loyalty tiers are determined by net delivered turnover and explicit ceilings") {
    const LoyaltyPolicy policy({Money{}, Money(100), Money(200)}, {0, 500, 1000}, 700);
    REQUIRE(policy.thresholds().size() == 3);
    REQUIRE(policy.discounts()[1] == 500);
    REQUIRE(policy.maximumDiscountBps() == 700);
    LoyaltyAccount account("alice");
    REQUIRE(policy.determineTier(account) == 0);
    REQUIRE(policy.nextTierProgress(account) == Money(100));
    account.recordDelivery("o", Money(100), "d");
    REQUIRE(policy.determineTier(account) == 1);
    REQUIRE(policy.calculateDiscount(account, Money(101)) == Money(5));
    account.recordDelivery("o2", Money(100), "d2");
    REQUIRE(policy.determineTier(account) == 2);
    REQUIRE_FALSE(policy.nextTierProgress(account));
    REQUIRE(policy.calculateDiscount(account, Money(100)) == Money(7));
    account.recordReturn("o", Money(100), "r");
    REQUIRE(policy.determineTier(account) == 1);
    REQUIRE_THROWS_AS(LoyaltyPolicy({}, {}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(LoyaltyPolicy({Money{}}, {0, 1}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(LoyaltyPolicy({Money{}, Money(0)}, {0, 1}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(LoyaltyPolicy({Money{}, Money(1)}, {2, 1}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(LoyaltyPolicy({Money(-1)}, {0}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(LoyaltyPolicy({Money{}}, {-1}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(LoyaltyPolicy({Money{}}, {10001}), InvalidDomainDataException);
}

TEST_CASE("Price changes use explicit time and category updates have all-or-nothing effects") {
    auto catalog = test::catalog();
    catalog.addVariant("shirt",
                       ProductVariant("shirt-red", "red", {{"color", "red"}}, 1, {1, 1, 1}));
    PriceList prices;
    REQUIRE(prices.minimumUnitPrice() == Money(1));
    prices.changePrice(catalog, "shirt-blue", Money(199), 10);
    prices.changePrice(catalog, "shirt-red", Money(101), 10);
    const auto snapshot = prices.price("shirt-blue");
    REQUIRE(snapshot.sku() == "shirt-blue");
    REQUIRE(snapshot.changedAt() == 10);
    REQUIRE(snapshot.revision() == 1);
    prices.applyCategoryPriceChange(catalog, "root", -5000, 11);
    REQUIRE(prices.price("shirt-blue").unitPrice() == Money(100));
    REQUIRE(prices.price("shirt-red").unitPrice() == Money(51));
    REQUIRE(snapshot.unitPrice() == Money(199));
    REQUIRE(prices.entries().size() == 2);
    REQUIRE(prices.revision() == 3);
    const auto before = prices;
    REQUIRE_THROWS_AS(prices.applyCategoryPriceChange(catalog, "root", -10000, 12),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(prices.applyCategoryPriceChange(catalog, "root", 1, 9),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(prices.changePrice(catalog, "shirt-blue", Money{}, 12),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(prices.changePrice(catalog, "shirt-blue", Money(20), 9),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(prices.removePriceForArchivedVariant(catalog, "shirt-blue"),
                      CatalogRuleException);
    REQUIRE(prices == before);
    prices.applyCategoryPriceChange(catalog, "other", 1000, 12);
    REQUIRE(prices.price("shirt-blue").unitPrice() == Money(100));
    prices.applyCategoryPriceChange(catalog, "root", 10000, 12);
    REQUIRE(prices.price("shirt-blue").unitPrice() == Money(200));
    catalog.archiveProduct("shirt");
    prices.applyCategoryPriceChange(catalog, "root", 1000, 13);
    REQUIRE(prices.price("shirt-blue").unitPrice() == Money(200));
    prices.removePriceForArchivedVariant(catalog, "shirt-blue");
    REQUIRE_THROWS_AS(prices.price("shirt-blue"), EntityNotFoundException);
    REQUIRE_THROWS_AS(PriceList(Money{}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(PriceEntry("s", Money(1), 0, 0), InvalidDomainDataException);
    REQUIRE_THROWS_AS(prices.applyCategoryPriceChange(catalog, "root", 10001, 14),
                      InvalidDomainDataException);
}
