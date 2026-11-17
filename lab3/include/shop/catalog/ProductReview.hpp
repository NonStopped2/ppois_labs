#pragma once

#include "shop/customers/Customer.hpp"
#include <set>
#include <string>
#include <string_view>

namespace shop {
enum class ReviewVisibility { Pending, Published, Hidden };
struct ModerationStatus {
    ReviewVisibility visibility = ReviewVisibility::Pending;
    std::string reason;
    bool operator==(const ModerationStatus&) const = default;
};
/** ProductReview.
 * Хранит оценку, текст и голоса по подтверждённой покупке. Редактирование сбрасывает голоса
 * и прежнюю модерацию.
 */
class ProductReview {
  public:
    ProductReview(std::string_view id, std::string_view customerId, std::string_view productId,
                  std::string_view orderId, int rating, std::string_view text);
    const std::string& id() const noexcept;
    const std::string& customerId() const noexcept;
    const std::string& productId() const noexcept;
    const std::string& orderId() const noexcept;
    int rating() const noexcept;
    const std::string& text() const noexcept;
    const ModerationStatus& moderationStatus() const noexcept;
    const std::set<std::string>& helpfulVoters() const noexcept;
    bool visible() const noexcept;
    void edit(std::string_view actor, int rating, std::string_view text);
    void publish();
    void hide(std::string_view reason);
    void voteHelpful(const Customer& voter);
    bool operator==(const ProductReview&) const = default;

  private:
    std::string id_;
    std::string customerId_;
    std::string productId_;
    std::string orderId_;
    int rating_;
    std::string text_;
    ModerationStatus moderationStatus_;
    std::set<std::string> helpfulVoters_;
};
} // namespace shop
