#include "../Fixtures.hpp"
#include "shop/common/CartRuleException.hpp"
#include "shop/common/DeliveryRuleException.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/common/PromotionRuleException.hpp"
#include "shop/common/QuoteChangedException.hpp"
#include "shop/common/ReturnRuleException.hpp"
#include "shop/pricing/PricingPolicy.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace shop;
namespace {
DeliveryOption standard() {
    return DeliveryOption("standard", "Стандарт", {"22"}, 10000, 1000, Money(100), Money(1000));
}
DeliveryPolicy deliveryPolicy() {
    return DeliveryPolicy({standard()}, 10, {{"standard", {100, 200}}});
}
struct QuoteFixture {
    ProductCatalog catalog = test::catalog();
    PriceList prices;
    std::map<std::string, Promotion> promotions;
    CouponBook coupons;
    LoyaltyAccount loyalty{"alice"};
    LoyaltyPolicy loyaltyPolicy;
    DeliveryPolicy delivery = deliveryPolicy();
    ReturnPolicy returns;
    Cart cart{"cart", "alice"};
    QuoteFixture() {
        prices.changePrice(catalog, "shirt-blue", Money(1000), 0);
        cart.addItem(catalog, "shirt-blue", 1, 0);
    }
    PricingContext context() const {
        return {catalog, prices, promotions, coupons, loyalty, loyaltyPolicy, delivery, returns};
    }
    PriceQuote quote() const {
        return PricingPolicy().makeQuote(cart, test::customer(), test::address(), "standard",
                                         context(), 10);
    }
};
} // namespace

TEST_CASE("Delivery compares address package limits and free-shipping thresholds") {
    const auto option = standard();
    const auto catalog = test::catalog();
    const std::vector<QuoteLine> lines{
        QuoteLine(catalog.variant("shirt-blue"), "Shirt", 2, Money(500), Money{}, true)};
    REQUIRE(option.id() == "standard");
    REQUIRE(option.name() == "Стандарт");
    REQUIRE(option.postalPrefixes().contains("22"));
    REQUIRE(option.maximumWeightGrams() == 10000);
    REQUIRE(option.maximumDimensionMm() == 1000);
    REQUIRE(option.baseFee() == Money(100));
    REQUIRE(option.freeFromSubtotal() == Money(1000));
    REQUIRE(option.accepts(test::address(), lines));
    REQUIRE_FALSE(option.accepts(test::address(), {}));
    REQUIRE_FALSE(option.accepts(Address("BY", "110001", "Город", "Улица", "1"), lines));
    REQUIRE_FALSE(
        DeliveryOption("x", "x", {}, 399, 1000, Money{}, Money{}).accepts(test::address(), lines));
    REQUIRE_FALSE(
        DeliveryOption("x", "x", {}, 10000, 10, Money{}, Money{}).accepts(test::address(), lines));
    REQUIRE(option.calculateFee(Money(999)) == Money(100));
    REQUIRE(option.calculateFee(Money(1000)) == Money{});
    const auto policy = deliveryPolicy();
    REQUIRE(policy.options().size() == 1);
    REQUIRE(policy.handlingDuration() == 10);
    REQUIRE(policy.transitDurations().at("standard").first == 100);
    REQUIRE(policy.estimateArrival("standard", 10) == TimeWindow(120, 220));
    REQUIRE(policy.cheapestEligible(test::address(), lines, Money(1000)).id() == "standard");
    REQUIRE_THROWS_AS(policy.cheapestEligible(test::address(), {}, Money{}), DeliveryRuleException);
    REQUIRE_THROWS_AS(DeliveryOption("x", "x", {"bad"}, 1, 1, Money{}, Money{}),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(DeliveryPolicy({}, 0, {}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(DeliveryPolicy({option, option}, 0, {{"standard", {1, 2}}}),
                      DuplicateEntityException);
    REQUIRE_THROWS_AS(DeliveryPolicy({option}, 0, {{"standard", {2, 1}}}),
                      InvalidDomainDataException);
}

TEST_CASE(
    "Return policy snapshots prevent later category changes from rewriting purchased rights") {
    auto catalog = test::catalog();
    const ReturnPolicy policy(100, {"other"});
    REQUIRE(policy.returnWindow() == 100);
    REQUIRE(policy.nonReturnableCategories().contains("other"));
    REQUIRE(policy.allowedReasons().contains(ReturnReason::Defective));
    REQUIRE(policy.restockableConditions().contains(ItemCondition::Opened));
    REQUIRE(policy.returnEligible(catalog, "shirt-blue"));
    catalog.bulkMoveProducts({"shirt"}, "other");
    REQUIRE_FALSE(policy.returnEligible(catalog, "shirt-blue"));
    REQUIRE_NOTHROW(policy.assertEligible(true, 10, ReturnReason::ChangedMind, 109));
    REQUIRE_THROWS_AS(policy.assertEligible(true, 10, ReturnReason::ChangedMind, 110),
                      ReturnRuleException);
    REQUIRE_THROWS_AS(policy.assertEligible(false, 10, ReturnReason::ChangedMind, 20),
                      ReturnRuleException);
    REQUIRE(policy.deadline(10) == 110);
    REQUIRE(policy.classifyRestockability(ItemCondition::Opened));
    REQUIRE_FALSE(policy.classifyRestockability(ItemCondition::Defective));
    REQUIRE_THROWS_AS(policy.classifyRestockability(static_cast<ItemCondition>(99)),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(ReturnPolicy(0), InvalidDomainDataException);
    REQUIRE_THROWS_AS(ReturnPolicy(1, {}, {}), InvalidDomainDataException);
    REQUIRE_THROWS_AS(ReturnPolicy(1, {}, {static_cast<ReturnReason>(99)}),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(ReturnPolicy(1, {}, {ReturnReason::Defective}, {ItemCondition::Defective}),
                      InvalidDomainDataException);
}

TEST_CASE("Quotes choose total payment rather than the largest nominal discount") {
    QuoteFixture fixture;
    fixture.promotions.emplace("sale", Promotion("sale", "sale", PromotionKind::FixedAmount, 50,
                                                 Money{}, TimeWindow(0, 1000)));
    auto quote = fixture.quote();
    REQUIRE(quote.total() == Money(1000));
    REQUIRE(quote.discountSource().kind == DiscountKind::None);
    REQUIRE(quote.goodsNet() == Money(1000));
    REQUIRE(quote.deliveryFee() == Money{});
    REQUIRE(quote.customerId() == "alice");
    REQUIRE(quote.deliveryAddress() == test::address());
    REQUIRE(quote.deliveryOptionId() == "standard");
    REQUIRE(quote.createdAt() == 10);
    REQUIRE(quote.validUntil() == 310);
    REQUIRE(quote.returnPolicy() == fixture.returns);
    REQUIRE_NOTHROW(quote.assertUsable(309));
    REQUIRE_THROWS_AS(quote.assertUsable(310), QuoteChangedException);
    fixture.promotions.emplace("better", Promotion("better", "better", PromotionKind::FixedAmount,
                                                   200, Money{}, TimeWindow(0, 50)));
    quote = fixture.quote();
    REQUIRE(quote.total() == Money(900));
    REQUIRE(quote.discountSource().id == "better");
    REQUIRE(quote.validUntil() == 50);
    const auto later = PricingPolicy().makeQuote(fixture.cart, test::customer(), test::address(),
                                                 "standard", fixture.context(), 11);
    REQUIRE(quote.sameCommercialTerms(later));
    REQUIRE_FALSE(quote == later);
    fixture.prices.changePrice(fixture.catalog, "shirt-blue", Money(1100), 10);
    REQUIRE_FALSE(quote.sameCommercialTerms(fixture.quote()));
}

TEST_CASE("Explicit coupons consume no quota in preview and loyalty is an alternative") {
    QuoteFixture fixture;
    const Promotion offer("coupon", "coupon", PromotionKind::FixedAmount, 150, Money{},
                          TimeWindow(0, 100), {}, false);
    fixture.promotions.emplace("coupon", offer);
    fixture.coupons.registerCode(Coupon("SAVE", "coupon", 1, 1, TimeWindow(0, 30)), offer);
    fixture.loyalty.recordDelivery("old", Money(500000), "old-delivery");
    fixture.cart.setCoupon("save");
    const auto quote = fixture.quote();
    REQUIRE(quote.discountSource().kind == DiscountKind::Coupon);
    REQUIRE(quote.discountSource().couponCode == "SAVE");
    REQUIRE(quote.total() == Money(950));
    REQUIRE(quote.validUntil() == 30);
    REQUIRE(fixture.coupons.heldUses().empty());
    fixture.cart.setCoupon(std::nullopt);
    REQUIRE(fixture.quote().discountSource().kind == DiscountKind::None);
    fixture.prices.changePrice(fixture.catalog, "shirt-blue", Money(2000), 0);
    REQUIRE(fixture.quote().discountSource().kind == DiscountKind::Loyalty);
    fixture.coupons.disableCode("save");
    fixture.cart.setCoupon("save");
    REQUIRE_THROWS_AS(fixture.quote(), PromotionRuleException);
}

TEST_CASE("Proportional allocation has stable ties and no intermediate overflow") {
    const auto catalog = test::catalog();
    const auto variant = catalog.variant("shirt-blue");
    const ProductVariant another("other", "Other", {}, 1, {1, 1, 1});
    const std::vector<QuoteLine> rows{QuoteLine(variant, "First", 1, Money(1), Money{}, true),
                                      QuoteLine(another, "Second", 1, Money(1), Money{}, true)};
    const PricingPolicy policy;
    REQUIRE(policy.quoteLifetime() == 300);
    REQUIRE(policy.maximumCartUnits() == 1000);
    REQUIRE(policy.preferLoyaltyOnTie());
    const auto allocation = policy.allocateDiscount(rows, {0, 1}, Money(1));
    REQUIRE(allocation[1] == Money(1));
    REQUIRE(allocation[0] == Money{});
    REQUIRE_THROWS_AS(policy.allocateDiscount(rows, {0, 0}, Money(1)), InvalidDomainDataException);
    REQUIRE_THROWS_AS(policy.allocateDiscount(rows, {2}, Money(1)), InvalidDomainDataException);
    REQUIRE_THROWS_AS(policy.allocateDiscount(rows, {0}, Money(2)), InvalidDomainDataException);
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const std::vector<QuoteLine> large{
        QuoteLine(variant, "First", 1, Money(maximum - 1), Money{}, true),
        QuoteLine(another, "Second", 1, Money(1), Money{}, true)};
    const auto parts = policy.allocateDiscount(large, {0, 1}, Money(maximum - 1));
    REQUIRE(parts[0].checkedAdd(parts[1]) == Money(maximum - 1));
    REQUIRE_THROWS_AS(PricingPolicy(0), InvalidDomainDataException);
    REQUIRE_THROWS_AS(policy.selectDiscount({}), CartRuleException);
    QuoteFixture fixture;
    fixture.returns = ReturnPolicy(100, {"root"});
    REQUIRE_FALSE(fixture.quote().lines().front().returnEligible());
    fixture.delivery =
        DeliveryPolicy({DeliveryOption("standard", "x", {"99"}, 1, 1, Money{}, Money{})}, 0,
                       {{"standard", {1, 2}}});
    REQUIRE_THROWS_AS(fixture.quote(), DeliveryRuleException);
}

TEST_CASE("Quote tie policy and invalid future or free purchases are explicit") {
    QuoteFixture fixture;
    fixture.prices.changePrice(fixture.catalog, "shirt-blue", Money(2000), 0);
    fixture.loyalty.recordDelivery("old", Money(500000), "delivered");
    fixture.promotions.emplace("z-sale", Promotion("z-sale", "sale", PromotionKind::FixedAmount,
                                                   100, Money{}, TimeWindow(0, 1000)));
    REQUIRE(fixture.quote().discountSource().kind == DiscountKind::Loyalty);
    REQUIRE(PricingPolicy(300, 1000, false)
                .makeQuote(fixture.cart, test::customer(), test::address(), "standard",
                           fixture.context(), 10)
                .discountSource()
                .kind == DiscountKind::Promotion);
    fixture.prices.changePrice(fixture.catalog, "shirt-blue", Money(2000), 11);
    REQUIRE_THROWS_AS(fixture.quote(), InvalidDomainDataException);
    fixture.prices.changePrice(fixture.catalog, "shirt-blue", Money(2000), 11);
    fixture.delivery =
        DeliveryPolicy({DeliveryOption("standard", "free", {}, 10000, 1000, Money{}, Money{})}, 0,
                       {{"standard", {1, 2}}});
    const Promotion full("full", "full", PromotionKind::Percentage, 10000, Money{},
                         TimeWindow(0, 100), {}, false);
    fixture.promotions.emplace("full", full);
    fixture.coupons.registerCode(Coupon("FULL", "full", 1, 1, TimeWindow(0, 100)), full);
    fixture.cart.setCoupon("FULL");
    REQUIRE_THROWS_AS(PricingPolicy().makeQuote(fixture.cart, test::customer(), test::address(),
                                                "standard", fixture.context(), 11),
                      CartRuleException);
}

TEST_CASE("Discount allocation agrees with independent integer largest-remainder arithmetic") {
    const ProductVariant first("a", "A", {}, 1, {1, 1, 1});
    const ProductVariant second("b", "B", {}, 1, {1, 1, 1});
    for (std::int64_t a = 1; a <= 12; ++a) {
        for (std::int64_t b = 1; b <= 12; ++b) {
            const std::vector<QuoteLine> lines{QuoteLine(first, "A", 1, Money(a), Money{}, true),
                                               QuoteLine(second, "B", 1, Money(b), Money{}, true)};
            for (std::int64_t discount = 0; discount <= a + b; ++discount) {
                const auto values = PricingPolicy::allocateDiscount(lines, {0, 1}, Money(discount));
                auto expected = discount * a / (a + b);
                const auto spare = discount - expected - discount * b / (a + b);
                if (spare && discount * a % (a + b) >= discount * b % (a + b)) {
                    ++expected;
                }
                REQUIRE(values[0] == Money(expected));
                REQUIRE(values[0].checkedAdd(values[1]) == Money(discount));
                REQUIRE(values[0] <= Money(a));
                REQUIRE(values[1] <= Money(b));
            }
        }
    }
}
