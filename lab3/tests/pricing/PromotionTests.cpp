#include "../Fixtures.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/EntityNotFoundException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/common/PromotionRuleException.hpp"
#include "shop/pricing/CouponBook.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

namespace {
std::vector<QuoteLine> lines(const ProductCatalog& catalog) {
    return {QuoteLine(catalog.variant("shirt-blue"), "Футболка", 3, Money(100), Money{}, true)};
}
Promotion offer() {
    return Promotion("sale", "Sale", PromotionKind::Percentage, 1000, Money{}, TimeWindow(10, 20),
                     {"root"}, false);
}
} // namespace

TEST_CASE("Quote lines own physical and commercial data without trusting current prices") {
    const auto catalog = test::catalog();
    const auto value = lines(catalog).front();
    REQUIRE(value.sku() == "shirt-blue");
    REQUIRE(value.title() == "Футболка");
    REQUIRE(value.quantity() == 3);
    REQUIRE(value.unitPrice() == Money(100));
    REQUIRE(value.netTotal() == Money(300));
    REQUIRE(value.packageSnapshot().weightGrams == 200);
    REQUIRE(value.packageSnapshot().attributes.at("color") == "blue");
    REQUIRE(value.returnEligible());
    REQUIRE_THROWS_AS(QuoteLine(catalog.variant("shirt-blue"), "title", 1, Money{}, Money{}, true),
                      InvalidDomainDataException);
}

TEST_CASE("Promotions filter categories respect time boundaries and cap fixed discounts") {
    const auto catalog = test::catalog();
    const auto rows = lines(catalog);
    const auto promotion = offer();
    REQUIRE(promotion.id() == "sale");
    REQUIRE(promotion.name() == "Sale");
    REQUIRE(promotion.kind() == PromotionKind::Percentage);
    REQUIRE(promotion.value() == 1000);
    REQUIRE(promotion.minimumSubtotal() == Money{});
    REQUIRE(promotion.window() == TimeWindow(10, 20));
    REQUIRE(promotion.categoryIds() == std::set<std::string>{"root"});
    REQUIRE_FALSE(promotion.automatic());
    REQUIRE(promotion.enabled());
    REQUIRE(promotion.calculateDiscount(rows, catalog, 10) == Money(30));
    REQUIRE(promotion.calculateDiscount(rows, catalog, 9) == Money{});
    REQUIRE(promotion.calculateDiscount(rows, catalog, 20) == Money{});
    REQUIRE_NOTHROW(promotion.assertApplicable(rows, catalog, 15));
    REQUIRE_THROWS_AS(promotion.assertApplicable(rows, catalog, 20), PromotionRuleException);
    REQUIRE(Promotion("p", "p", PromotionKind::FixedAmount, 999, Money{}, TimeWindow(0, 100))
                .calculateDiscount(rows, catalog, 10) == Money(300));
    REQUIRE(Promotion("p", "p", PromotionKind::Percentage, 1000, Money(301), TimeWindow(0, 100))
                .calculateDiscount(rows, catalog, 10) == Money{});
    REQUIRE(
        Promotion("p", "p", PromotionKind::Percentage, 1000, Money{}, TimeWindow(0, 100), {"other"})
            .calculateDiscount(rows, catalog, 10) == Money{});
    REQUIRE(Promotion("p", "p", PromotionKind::Percentage, 1000, Money{}, TimeWindow(0, 100), {},
                      true, false)
                .eligibleLines(rows, catalog, 10)
                .empty());
    REQUIRE_THROWS_AS(
        Promotion("p", "p", static_cast<PromotionKind>(9), 100, Money{}, TimeWindow(0, 1)),
        InvalidDomainDataException);
    REQUIRE_THROWS_AS(
        Promotion("p", "p", PromotionKind::Percentage, 10001, Money{}, TimeWindow(0, 1)),
        InvalidDomainDataException);
}

TEST_CASE("Coupon holds use both limits and survive expiry until confirmed payment") {
    const auto promotion = offer();
    CouponBook book;
    const Coupon coupon(" save-10 ", "sale", 2, 1, TimeWindow(10, 20));
    REQUIRE(coupon.code() == "SAVE-10");
    REQUIRE(coupon.promotionId() == "sale");
    REQUIRE(coupon.totalUseLimit() == 2);
    REQUIRE(coupon.perCustomerLimit() == 1);
    REQUIRE(coupon.window() == TimeWindow(10, 20));
    book.registerCode(coupon, promotion);
    REQUIRE_THROWS_AS(book.registerCode(coupon, promotion), DuplicateEntityException);
    REQUIRE_THROWS_AS(
        book.registerCode(Coupon("wrong", "other", 1, 1, TimeWindow(0, 100)), promotion),
        PromotionRuleException);
    REQUIRE(book.coupons().size() == 1);
    book.reserveUse("save-10", test::customer(), promotion, "o1", 10);
    REQUIRE(book.heldUses().size() == 1);
    REQUIRE(book.usageSummary("SAVE-10", "alice").available == 0);
    REQUIRE(book.usageSummary("SAVE-10", "bob").available == 1);
    const auto before = book;
    REQUIRE_THROWS_AS(book.reserveUse("save-10", test::customer(), promotion, "o1", 11),
                      DuplicateEntityException);
    REQUIRE_THROWS_AS(book.reserveUse("save-10", test::customer(), promotion, "o2", 11),
                      PromotionRuleException);
    REQUIRE_THROWS_AS(book.confirmUse("o1", 9), PromotionRuleException);
    REQUIRE(book == before);
    book.disableCode("save-10");
    REQUIRE_FALSE(book.coupon("save-10").enabled());
    book.confirmUse("o1", 21);
    REQUIRE(book.heldUses().empty());
    REQUIRE(book.confirmedUses().size() == 1);
    REQUIRE(book.usageSummary("save-10", "alice").customerConfirmed == 1);
    REQUIRE_THROWS_AS(book.releaseUse("o1"), PromotionRuleException);
    REQUIRE_THROWS_AS(book.reserveUse("save-10", test::customer("bob"), promotion, "o2", 12),
                      PromotionRuleException);
    REQUIRE_THROWS_AS(book.releaseUse("missing"), EntityNotFoundException);
}

TEST_CASE("Coupon release and global exhaustion are distinct from paid use") {
    const auto promotion = offer();
    CouponBook book;
    book.registerCode(Coupon("c", "sale", 1, 1, TimeWindow(10, 20)), promotion);
    book.reserveUse("c", test::customer(), promotion, "one", 10);
    REQUIRE_THROWS_AS(book.reserveUse("c", test::customer("bob"), promotion, "two", 10),
                      PromotionRuleException);
    book.releaseUse("one");
    REQUIRE(book.usageSummary("c", "alice").available == 1);
    book.reserveUse("c", test::customer("bob"), promotion, "two", 10);
    book.confirmUse("two", 11);
    REQUIRE(book.usageSummary("c", "alice").confirmed == 1);
    REQUIRE(book.usageSummary("c", "bob").customerConfirmed == 1);
    REQUIRE_THROWS_AS(book.reserveUse("c", test::customer(), promotion, "three", 11),
                      PromotionRuleException);
    REQUIRE_THROWS_AS(Coupon("?", "sale", 1, 1, TimeWindow(0, 100)), InvalidDomainDataException);
    REQUIRE_THROWS_AS(Coupon(std::string(65, 'a'), "sale", 1, 1, TimeWindow(0, 100)),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(Coupon("x", "sale", 1, 2, TimeWindow(0, 100)), InvalidDomainDataException);
    REQUIRE_THROWS_AS(book.reserveUse("c", test::customer(), promotion, "four", 20),
                      PromotionRuleException);
}
