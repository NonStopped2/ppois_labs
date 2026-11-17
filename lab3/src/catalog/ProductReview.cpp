#include "shop/catalog/ProductReview.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/ReviewRuleException.hpp"
#include "shop/common/Validation.hpp"

namespace shop {
ProductReview::ProductReview(std::string_view id, std::string_view customerId,
                             std::string_view productId, std::string_view orderId, int rating,
                             std::string_view text)
    : id_(detail::text(id, "отзыв")), customerId_(detail::text(customerId, "автор отзыва")),
      productId_(detail::text(productId, "продукт отзыва")),
      orderId_(detail::text(orderId, "покупка отзыва")), rating_(rating),
      text_(detail::text(text, "текст отзыва")) {
    detail::check<ReviewRuleException>(rating >= 1 && rating <= 5,
                                       "Оценка отзыва должна быть от 1 до 5");
}
const std::string& ProductReview::id() const noexcept {
    return id_;
}
const std::string& ProductReview::customerId() const noexcept {
    return customerId_;
}
const std::string& ProductReview::productId() const noexcept {
    return productId_;
}
const std::string& ProductReview::orderId() const noexcept {
    return orderId_;
}
int ProductReview::rating() const noexcept {
    return rating_;
}
const std::string& ProductReview::text() const noexcept {
    return text_;
}
const ModerationStatus& ProductReview::moderationStatus() const noexcept {
    return moderationStatus_;
}
const std::set<std::string>& ProductReview::helpfulVoters() const noexcept {
    return helpfulVoters_;
}
bool ProductReview::visible() const noexcept {
    return moderationStatus_.visibility == ReviewVisibility::Published;
}
// Отзыв с новым текстом возвращается на модерацию, а прежние голоса сбрасываются.
void ProductReview::edit(std::string_view actor, int newRating, std::string_view newText) {
    detail::check<ReviewRuleException>(actor == customerId_ && newRating >= 1 && newRating <= 5,
                                       "Чужой отзыв или недопустимая оценка");
    auto replacement = detail::text(newText, "текст отзыва");
    text_.swap(replacement);
    rating_ = newRating;
    helpfulVoters_.clear();
    moderationStatus_.reason.clear();
    moderationStatus_.visibility = ReviewVisibility::Pending;
}
void ProductReview::publish() {
    moderationStatus_.reason.clear();
    moderationStatus_.visibility = ReviewVisibility::Published;
}
void ProductReview::hide(std::string_view reason) {
    auto explanation = detail::text(reason, "причина скрытия отзыва");
    moderationStatus_.reason.swap(explanation);
    moderationStatus_.visibility = ReviewVisibility::Hidden;
}
void ProductReview::voteHelpful(const Customer& voter) {
    detail::check<ReviewRuleException>(visible() && voter.id() != customerId_,
                                       "Голосовать можно только за видимый чужой отзыв");
    detail::check<DuplicateEntityException>(helpfulVoters_.insert(voter.id()).second,
                                            "Покупатель уже голосовал за отзыв");
}
} // namespace shop
