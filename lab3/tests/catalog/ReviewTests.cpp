#include "../PurchaseFixtures.hpp"
#include "../StoreFixtures.hpp"
#include "shop/application/ReturnProcessing.hpp"
#include "shop/catalog/ReviewBook.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/EntityNotFoundException.hpp"
#include "shop/common/InvalidDomainDataException.hpp"
#include "shop/common/ReviewRuleException.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace shop;

TEST_CASE(
    "Review editing clears votes and moderation while preventing forged authors and self-votes") {
    ProductReview review("r", "alice", "shirt", "order", 5, "Хороший товар");
    REQUIRE(review.id() == "r");
    REQUIRE(review.customerId() == "alice");
    REQUIRE(review.productId() == "shirt");
    REQUIRE(review.orderId() == "order");
    REQUIRE(review.rating() == 5);
    REQUIRE(review.text() == "Хороший товар");
    REQUIRE(review.moderationStatus().visibility == ReviewVisibility::Pending);
    REQUIRE_FALSE(review.visible());
    REQUIRE_THROWS_AS(review.voteHelpful(test::customer("bob")), ReviewRuleException);
    review.publish();
    review.voteHelpful(test::customer("bob"));
    REQUIRE(review.helpfulVoters().size() == 1);
    const auto before = review;
    REQUIRE_THROWS_AS(review.voteHelpful(test::customer("bob")), DuplicateEntityException);
    REQUIRE_THROWS_AS(review.voteHelpful(test::customer()), ReviewRuleException);
    REQUIRE_THROWS_AS(review.edit("bob", 1, "Другой текст"), ReviewRuleException);
    REQUIRE_THROWS_AS(review.edit("alice", 0, "Другой текст"), ReviewRuleException);
    REQUIRE(review == before);
    review.edit("alice", 4, "Исправление");
    REQUIRE(review.helpfulVoters().empty());
    REQUIRE_FALSE(review.visible());
    review.hide("Нужны подробности");
    REQUIRE(review.moderationStatus().visibility == ReviewVisibility::Hidden);
    REQUIRE(review.moderationStatus().reason == "Нужны подробности");
    review.publish();
    REQUIRE(review.visible());
    REQUIRE(review.moderationStatus().reason.empty());
    REQUIRE_THROWS_AS(ProductReview("r", "a", "p", "o", 6, "x"), ReviewRuleException);
}

TEST_CASE("Verified reviews require retained delivered units and have deterministic visible rating "
          "pagination") {
    const auto order = test::deliveredOrder();
    const auto product = test::product();
    ReviewBook reviews;
    REQUIRE_FALSE(reviews.ratingSummary("shirt").average());
    REQUIRE_THROWS_AS(
        reviews.addVerifiedReview("r", test::customer(), test::paidOrder(), product, {}, 5, "x"),
        ReviewRuleException);
    REQUIRE_THROWS_AS(reviews.addVerifiedReview("r", test::customer(), order, product,
                                                {{"shirt-blue", 3}}, 5, "x"),
                      ReviewRuleException);
    reviews.addVerifiedReview("r", test::customer(), order, product, {{"shirt-blue", 1}}, 5,
                              "Нравится");
    REQUIRE(reviews.revision() == 1);
    REQUIRE(reviews.reviews().size() == 1);
    REQUIRE(reviews.ratingSummary("shirt").count == 0);
    reviews.moderate("r", true);
    reviews.voteHelpful("r", test::customer("bob"));
    const auto summary = reviews.ratingSummary("shirt");
    REQUIRE(summary.count == 1);
    REQUIRE(summary.distribution[4] == 1);
    REQUIRE(summary.totalScore == 5);
    REQUIRE(summary.average() == std::make_pair(std::int64_t{5}, std::int64_t{1}));
    REQUIRE(reviews.verifiedReviewsFor("shirt").front().id() == "r");
    REQUIRE(reviews.verifiedReviewsFor("shirt", ReviewSort::Id, 1).empty());
    REQUIRE(reviews.verifiedReviewsFor("shirt", ReviewSort::Id, 0, 0).empty());
    REQUIRE_THROWS_AS(
        reviews.addVerifiedReview("other", test::customer(), order, product, {}, 4, "Второй"),
        DuplicateEntityException);
    const auto before = reviews;
    REQUIRE_THROWS_AS(reviews.moderate("r", true, "Причина скрытия"), InvalidDomainDataException);
    REQUIRE(reviews == before);
    reviews.edit("r", "alice", 3, "Изменилось мнение");
    REQUIRE(reviews.ratingSummary("shirt").count == 0);
    reviews.moderate("r", false, "Проверка");
    REQUIRE(reviews.verifiedReviewsFor("shirt").empty());
    REQUIRE_THROWS_AS(reviews.verifiedReviewsFor("shirt", static_cast<ReviewSort>(99)),
                      InvalidDomainDataException);
}

TEST_CASE("Store reviews remain visible after returns but a fully returned product cannot grant a "
          "new review") {
    auto state = test::store();
    const auto orderId = test::place(state);
    test::pay(state, orderId);
    test::ship(state, orderId);
    OrderProcessing(state).confirmDelivery("alice", orderId, "deliver");
    const auto review = state.addReview("alice", orderId, "shirt", 5, "Нравится");
    state.moderateReview(review, true);
    state.voteHelpful("bob", review);
    REQUIRE(state.reviews().ratingSummary("shirt").count == 1);
    REQUIRE_THROWS_AS(state.voteHelpful("unknown", review), EntityNotFoundException);
    ReturnProcessing returns(state);
    const auto request =
        returns.open("alice", orderId, {{"shirt-blue", 3, ReturnReason::ChangedMind}}, "open");
    returns.inspect(request, {InspectionResult("shirt-blue", 3, 3, 3, ItemCondition::Opened)},
                    "inspect");
    returns.complete(request, "complete");
    REQUIRE(state.reviews().ratingSummary("shirt").count == 1);
    REQUIRE_THROWS_AS(state.addReview("alice", orderId, "shirt", 4, "Ещё"), ReviewRuleException);
    state.editReview("alice", review, 4, "Обновление");
    REQUIRE(state.reviews().ratingSummary("shirt").count == 0);
    state.moderateReview(review, false, "Проверяется");
    REQUIRE(state.reviews().review(review).moderationStatus().reason == "Проверяется");
}

TEST_CASE("Product comparison distinguishes missing incompatible and unpriced characteristics") {
    auto catalog = test::catalog();
    auto product = test::product("other", "other-blue");
    product.addVariant(ProductVariant("other-red", "red", {{"color", "red"}}, 1, {1, 1, 1}));
    catalog.addProduct(product);
    catalog.publish("other");
    Product number("number", "other", "Числовой", "",
                   {ProductAttribute("color", AttributeKind::Integer, true)});
    number.addVariant(ProductVariant("number-sku", "N", {{"color", "1"}}, 1, {1, 1, 1}));
    catalog.addProduct(number);
    catalog.publish("number");
    Product absent("absent", "other", "Без цвета", "");
    absent.addVariant(ProductVariant("no-color", "None", {}, 1, {1, 1, 1}));
    catalog.addProduct(absent);
    catalog.publish("absent");
    Inventory stock(Warehouse("w", "Склад", test::address()));
    stock.addStockItem(catalog, "shirt-blue", 0);
    stock.applyStockCount("shirt-blue", 5);
    stock.reserve("r", "o", {{"shirt-blue", 2}}, 0, 100);
    PriceList prices;
    prices.changePrice(catalog, "shirt-blue", Money(100), 0);
    prices.changePrice(catalog, "other-blue", Money(50), 0);
    prices.changePrice(catalog, "other-red", Money(200), 0);
    auto comparison =
        catalog.compareProducts({"shirt", "other", "number", "absent"}, prices, stock);
    REQUIRE(comparison.products.at("shirt").availableBySku.at("shirt-blue") == 3);
    REQUIRE(comparison.products.at("other").priceRange == std::make_pair(Money(50), Money(200)));
    REQUIRE_FALSE(comparison.products.at("number").priceRange);
    REQUIRE_FALSE(comparison.attributes.at("color").comparable);
    REQUIRE_FALSE(comparison.attributes.at("color").kinds.at("absent"));
    REQUIRE(comparison.attributes.at("color").values.at("other") ==
            std::set<std::string>{"blue", "red"});
    catalog.setVariantAvailability("other-red", false);
    catalog.archiveProduct("shirt");
    comparison = catalog.compareProducts({"shirt", "other"}, prices, stock);
    REQUIRE(comparison.attributes.at("color").comparable);
    REQUIRE_FALSE(comparison.products.at("shirt").priceRange);
    REQUIRE(comparison.products.at("other").priceRange == std::make_pair(Money(50), Money(50)));
    REQUIRE(comparison.products.at("shirt").availableBySku.at("shirt-blue") == 0);
    REQUIRE_THROWS_AS(catalog.compareProducts({"shirt"}, prices, stock),
                      InvalidDomainDataException);
    REQUIRE_THROWS_AS(catalog.compareProducts({"shirt", "shirt"}, prices, stock),
                      DuplicateEntityException);
}
