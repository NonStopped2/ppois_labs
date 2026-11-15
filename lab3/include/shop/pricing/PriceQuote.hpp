#pragma once
#include "shop/common/Address.hpp"
#include "shop/pricing/QuoteLine.hpp"
#include "shop/returns/ReturnPolicy.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shop {
enum class DiscountKind { None, Promotion, Coupon, Loyalty };
struct DiscountSource {
    DiscountKind kind = DiscountKind::None;
    std::string id;
    std::optional<std::string> couponCode;
    bool operator==(const DiscountSource&) const = default;
};

/** Immutable terms accepted by a buyer; no inventory is held by an offer. */
class PriceQuote {
  public:
    PriceQuote(std::string_view customerId, std::vector<QuoteLine> lines, Address deliveryAddress,
               std::string_view deliveryOptionId, Money deliveryFee, DiscountSource discountSource,
               Timestamp createdAt, Timestamp validUntil, ReturnPolicy returnPolicy);
    const std::string& customerId() const noexcept;
    const std::vector<QuoteLine>& lines() const noexcept;
    const Address& deliveryAddress() const noexcept;
    const std::string& deliveryOptionId() const noexcept;
    Money deliveryFee() const noexcept;
    const DiscountSource& discountSource() const noexcept;
    Timestamp createdAt() const noexcept;
    Timestamp validUntil() const noexcept;
    Money total() const noexcept;
    Money goodsNet() const;
    const ReturnPolicy& returnPolicy() const noexcept;
    void assertUsable(Timestamp now) const;
    bool sameCommercialTerms(const PriceQuote& other) const;
    bool operator==(const PriceQuote&) const = default;

  private:
    std::string customerId_;
    std::vector<QuoteLine> lines_;
    Address deliveryAddress_;
    std::string deliveryOptionId_;
    Money deliveryFee_;
    DiscountSource discountSource_;
    Timestamp createdAt_;
    Timestamp validUntil_;
    Money total_;
    ReturnPolicy returnPolicySnapshot_;
};
} // namespace shop
