#include "shop/pricing/PricingPolicy.hpp"
#include "shop/common/CartRuleException.hpp"
#include "shop/common/DeliveryRuleException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/PromotionRuleException.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>
#include <numeric>
#include <set>

namespace shop {
namespace {
Money sumGross(const std::vector<QuoteLine>& lines) {
    return std::accumulate(lines.begin(), lines.end(), Money{}, [](Money sum, const auto& line) {
        return sum.checkedAdd(line.allocation().grossTotal());
    });
}
std::vector<std::size_t> allLines(const std::vector<QuoteLine>& lines) {
    std::vector<std::size_t> indices(lines.size());
    std::iota(indices.begin(), indices.end(), std::size_t{0});
    return indices;
}
} // namespace
PricingPolicy::PricingPolicy(std::int64_t quoteLifetime, std::int64_t maximumCartUnits,
                             bool preferLoyaltyOnTie)
    : quoteLifetime_(quoteLifetime), maximumCartUnits_(maximumCartUnits),
      preferLoyaltyOnTie_(preferLoyaltyOnTie) {
    detail::require(quoteLifetime > 0 && maximumCartUnits > 0,
                    "Некорректные пределы расчёта корзины");
}
std::int64_t PricingPolicy::quoteLifetime() const noexcept {
    return quoteLifetime_;
}
std::int64_t PricingPolicy::maximumCartUnits() const noexcept {
    return maximumCartUnits_;
}
bool PricingPolicy::preferLoyaltyOnTie() const noexcept {
    return preferLoyaltyOnTie_;
}

// Распределение по строкам сохраняет общую сумму скидки с точностью до копейки.
std::vector<Money> PricingPolicy::allocateDiscount(const std::vector<QuoteLine>& lines,
                                                   const std::vector<std::size_t>& eligible,
                                                   Money discount) {
    std::set<std::size_t> seen;
    Money eligibleGross;
    for (const auto index : eligible) {
        detail::require(index < lines.size() && seen.insert(index).second,
                        "Неверный или повторный индекс скидки");
        eligibleGross = eligibleGross.checkedAdd(lines[index].allocation().grossTotal());
    }
    detail::require(discount >= Money{} && discount <= eligibleGross,
                    "Скидка превышает применимую товарную сумму");
    std::vector<Money> result(lines.size());
    if (discount == Money{}) {
        return result;
    }
    Money distributed;
    std::vector<std::pair<std::int64_t, std::size_t>> remainders;
    for (const auto index : eligible) {
        const auto [part, remainder] = discount.fractionParts(
            lines[index].allocation().grossTotal().kopecks(), eligibleGross.kopecks());
        result[index] = part;
        distributed = distributed.checkedAdd(part);
        remainders.emplace_back(remainder, index);
    }
    std::sort(remainders.begin(), remainders.end(), [&lines](const auto& left, const auto& right) {
        return left.first != right.first ? left.first > right.first
                                         : lines[left.second].sku() < lines[right.second].sku();
    });
    const auto extra = discount.checkedSubtract(distributed).kopecks();
    for (std::int64_t index = 0; index < extra; ++index) {
        const auto selected = remainders.at(static_cast<std::size_t>(index)).second;
        result[selected] = result[selected].checkedAdd(Money(1));
    }
    return result;
}

PriceQuote PricingPolicy::selectDiscount(const std::vector<PriceQuote>& candidates) const {
    detail::check<CartRuleException>(!candidates.empty(),
                                     "Нет допустимого положительного итога заказа");
    const auto rank = [this](DiscountKind kind) {
        if (kind == DiscountKind::None) {
            return 0;
        }
        const bool loyalty = kind == DiscountKind::Loyalty;
        return loyalty == preferLoyaltyOnTie_ ? 1 : 2;
    };
    const auto best = std::min_element(
        candidates.begin(), candidates.end(), [&rank](const auto& left, const auto& right) {
            if (left.total() != right.total()) {
                return left.total() < right.total();
            }
            const auto leftRank = rank(left.discountSource().kind);
            const auto rightRank = rank(right.discountSource().kind);
            return leftRank != rightRank ? leftRank < rightRank
                                         : left.discountSource().id < right.discountSource().id;
        });
    return *best;
}

// Предложение собирается из актуальных данных. Резерв и квота появятся только при оформлении.
PriceQuote PricingPolicy::makeQuote(const Cart& cart, const Customer& customer,
                                    const Address& destination, std::string_view deliveryOptionId,
                                    const PricingContext& context, Timestamp now) const {
    customer.assertMayCheckout();
    cart.assertCheckoutReady(customer.id(), maximumCartUnits_);
    detail::require(context.loyaltyAccount.customerId() == customer.id(),
                    "Счёт лояльности принадлежит другому покупателю");
    std::vector<QuoteLine> raw;
    for (const auto& [sku, line] : cart.lines()) {
        const auto item = context.catalog.productForSku(sku);
        const auto option = context.catalog.variant(sku);
        detail::check<CartRuleException>(
            item.status() == ProductStatus::Published && option.saleEnabled(), "Товар недоступен");
        detail::require(now >= line.addedAt(), "Товар добавлен в будущем");
        const auto currentPrice = context.prices.price(sku);
        detail::require(currentPrice.changedAt() <= now, "Цена установлена в будущем");
        raw.emplace_back(option, item.title(), line.quantity(), currentPrice.unitPrice(), Money{},
                         context.returnPolicy.returnEligible(context.catalog, sku));
    }
    const auto delivery = context.deliveryPolicy.option(deliveryOptionId);
    detail::check<DeliveryRuleException>(delivery.accepts(destination, raw),
                                         "Выбранная доставка недоступна");
    const auto gross = sumGross(raw);
    const auto defaultExpiry = detail::addNonnegative(now, quoteLifetime_);
    std::vector<PriceQuote> candidates;
    const auto addCandidate = [&](Money discount, const std::vector<std::size_t>& eligible,
                                  const DiscountSource& source, Timestamp expires) {
        const auto net = gross.checkedSubtract(discount);
        const auto fee = delivery.calculateFee(net);
        if (net.checkedAdd(fee) == Money{}) {
            return;
        }
        const auto allocation = allocateDiscount(raw, eligible, discount);
        std::vector<QuoteLine> discounted;
        for (std::size_t index = 0; index < raw.size(); ++index) {
            discounted.push_back(raw[index].withDiscount(allocation[index]));
        }
        candidates.emplace_back(customer.id(), std::move(discounted), destination, deliveryOptionId,
                                fee, source, now, expires, context.returnPolicy);
    };
    if (cart.couponCode()) {
        const auto entry = context.coupons.coupon(*cart.couponCode());
        const auto usage = context.coupons.usageSummary(entry.code(), customer.id());
        entry.assertRedeemable(customer, usage.held + usage.confirmed,
                               usage.customerHeld + usage.customerConfirmed, now);
        const auto& promotion = detail::get(context.promotions, entry.promotionId());
        promotion.assertApplicable(raw, context.catalog, now);
        addCandidate(
            promotion.calculateDiscount(raw, context.catalog, now),
            promotion.eligibleLines(raw, context.catalog, now),
            {DiscountKind::Coupon, promotion.id(), entry.code()},
            std::min({defaultExpiry, promotion.window().endsAt(), entry.window().endsAt()}));
        return selectDiscount(candidates);
    }
    addCandidate(Money{}, allLines(raw), {}, defaultExpiry);
    for (const auto& [id, promotion] : context.promotions) {
        if (!promotion.automatic()) {
            continue;
        }
        const auto discount = promotion.calculateDiscount(raw, context.catalog, now);
        if (discount == Money{}) {
            continue;
        }
        addCandidate(discount, promotion.eligibleLines(raw, context.catalog, now),
                     {DiscountKind::Promotion, id, std::nullopt},
                     std::min(defaultExpiry, promotion.window().endsAt()));
    }
    const auto loyalDiscount =
        context.loyaltyPolicy.calculateDiscount(context.loyaltyAccount, gross);
    if (loyalDiscount > Money{}) {
        addCandidate(loyalDiscount, allLines(raw), {DiscountKind::Loyalty, "loyalty", std::nullopt},
                     defaultExpiry);
    }
    return selectDiscount(candidates);
}
} // namespace shop
