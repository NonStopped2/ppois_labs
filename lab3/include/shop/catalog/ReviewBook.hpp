#pragma once

#include "shop/catalog/Product.hpp"
#include "shop/catalog/ProductReview.hpp"
#include "shop/orders/Order.hpp"
#include <array>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace shop {
enum class ReviewSort { Helpful, HighestRating, LowestRating, Id };
struct RatingSummary {
    std::array<std::int64_t, 5> distribution{};
    std::int64_t count = 0;
    std::int64_t totalScore = 0;
    /** Exact average numerator/denominator; no fictitious zero rating for an empty set. */
    std::optional<std::pair<std::int64_t, std::int64_t>> average() const;
};
class ReviewBook {
  public:
    void addVerifiedReview(std::string_view id, const Customer& customer, const Order& order,
                           const Product& product,
                           const std::map<std::string, std::int64_t>& returnedQuantities,
                           int rating, std::string_view text);
    void edit(std::string_view id, std::string_view actor, int rating, std::string_view text);
    void moderate(std::string_view id, bool publish, std::string_view reason = {});
    void voteHelpful(std::string_view id, const Customer& voter);
    RatingSummary ratingSummary(std::string_view productId) const;
    std::vector<ProductReview> verifiedReviewsFor(std::string_view productId,
                                                  ReviewSort sort = ReviewSort::Helpful,
                                                  std::size_t offset = 0,
                                                  std::size_t limit = 20) const;
    ProductReview review(std::string_view id) const;
    const std::map<std::string, ProductReview>& reviews() const noexcept;
    std::int64_t revision() const noexcept;
    bool operator==(const ReviewBook&) const = default;

  private:
    friend class SnapshotCodec;
    void replace(ProductReview candidate);
    std::map<std::string, ProductReview> reviews_;
    std::map<std::pair<std::string, std::string>, std::string> purchaseReviewIndex_;
    std::int64_t revision_ = 0;
};
} // namespace shop
