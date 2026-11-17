#include "shop/catalog/ReviewBook.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/ReviewRuleException.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {
std::optional<std::pair<std::int64_t, std::int64_t>> RatingSummary::average() const {
    return count == 0 ? std::nullopt : std::optional(std::make_pair(totalScore, count));
}
// Право на отзыв следует из подтверждённой покупки с оставшимися у покупателя единицами.
void ReviewBook::addVerifiedReview(std::string_view id, const Customer& customer,
                                   const Order& order, const Product& product,
                                   const std::map<std::string, std::int64_t>& returnedQuantities,
                                   int rating, std::string_view text) {
    order.assertOwner(customer.id());
    detail::check<ReviewRuleException>(order.status() == OrderStatus::Delivered,
                                       "Отзыв требует полученного товара");
    bool retained = false;
    for (const auto& entry : order.lines()) {
        const bool belongs =
            std::any_of(product.variants().begin(), product.variants().end(),
                        [&entry](const auto& variant) { return variant.sku() == entry.sku(); });
        if (!belongs) {
            continue;
        }
        const auto found = returnedQuantities.find(entry.sku());
        const auto returned = found == returnedQuantities.end() ? 0 : found->second;
        detail::check<ReviewRuleException>(returned >= 0 && returned <= entry.quantity(),
                                           "Неверное возвращённое количество покупки");
        retained = retained || returned < entry.quantity();
    }
    detail::check<ReviewRuleException>(retained,
                                       "Нет полученного и не полностью возвращённого товара");
    const auto indexKey = std::make_pair(customer.id(), product.id());
    detail::check<DuplicateEntityException>(!purchaseReviewIndex_.contains(indexKey),
                                            "Покупатель уже оставлял отзыв на продукт");
    ProductReview entry(id, customer.id(), product.id(), order.id(), rating, text);
    auto candidate = reviews_;
    auto index = purchaseReviewIndex_;
    detail::check<DuplicateEntityException>(candidate.emplace(entry.id(), entry).second,
                                            "Повторный ID отзыва");
    index.emplace(indexKey, entry.id());
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    reviews_.swap(candidate);
    purchaseReviewIndex_.swap(index);
    revision_ = nextRevision;
}
void ReviewBook::replace(ProductReview candidate) {
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    auto values = reviews_;
    values.at(candidate.id()) = std::move(candidate);
    reviews_.swap(values);
    revision_ = nextRevision;
}
void ReviewBook::edit(std::string_view id, std::string_view actor, int rating,
                      std::string_view text) {
    auto candidate = review(id);
    candidate.edit(actor, rating, text);
    replace(std::move(candidate));
}
void ReviewBook::moderate(std::string_view id, bool publish, std::string_view reason) {
    auto candidate = review(id);
    if (publish) {
        detail::require(detail::text(reason, "причина", true).empty(),
                        "Публикация не содержит причины скрытия");
        candidate.publish();
    } else {
        candidate.hide(reason);
    }
    replace(std::move(candidate));
}
void ReviewBook::voteHelpful(std::string_view id, const Customer& voter) {
    auto candidate = review(id);
    candidate.voteHelpful(voter);
    replace(std::move(candidate));
}
RatingSummary ReviewBook::ratingSummary(std::string_view productId) const {
    RatingSummary result;
    for (const auto& [id, entry] : reviews_) {
        if (entry.productId() != productId || !entry.visible()) {
            continue;
        }
        ++result.distribution[static_cast<std::size_t>(entry.rating() - 1)];
        result.count = detail::addNonnegative(result.count, 1);
        result.totalScore = detail::addNonnegative(result.totalScore, entry.rating());
    }
    return result;
}
std::vector<ProductReview> ReviewBook::verifiedReviewsFor(std::string_view productId,
                                                          ReviewSort sort, std::size_t offset,
                                                          std::size_t limit) const {
    detail::require(sort == ReviewSort::Helpful || sort == ReviewSort::HighestRating ||
                        sort == ReviewSort::LowestRating || sort == ReviewSort::Id,
                    "Неизвестная сортировка отзывов");
    std::vector<ProductReview> result;
    for (const auto& [id, entry] : reviews_) {
        if (entry.productId() == productId && entry.visible()) {
            result.push_back(entry);
        }
    }
    std::sort(result.begin(), result.end(), [sort](const auto& left, const auto& right) {
        if (sort == ReviewSort::Helpful &&
            left.helpfulVoters().size() != right.helpfulVoters().size()) {
            return left.helpfulVoters().size() > right.helpfulVoters().size();
        }
        if (sort == ReviewSort::HighestRating && left.rating() != right.rating()) {
            return left.rating() > right.rating();
        }
        if (sort == ReviewSort::LowestRating && left.rating() != right.rating()) {
            return left.rating() < right.rating();
        }
        return left.id() < right.id();
    });
    if (offset >= result.size() || limit == 0) {
        return {};
    }
    const auto count = std::min(limit, result.size() - offset);
    const auto first = result.begin() + static_cast<std::ptrdiff_t>(offset);
    return {first, first + static_cast<std::ptrdiff_t>(count)};
}
ProductReview ReviewBook::review(std::string_view id) const {
    return detail::get(reviews_, id);
}
const std::map<std::string, ProductReview>& ReviewBook::reviews() const noexcept {
    return reviews_;
}
std::int64_t ReviewBook::revision() const noexcept {
    return revision_;
}
} // namespace shop
