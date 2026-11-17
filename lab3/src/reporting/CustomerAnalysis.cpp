#include "ReportFacts.hpp"
#include "shop/reporting/SalesReport.hpp"
#include <algorithm>

namespace shop {
std::optional<ExactRatio> SalesReport::repeatPurchaseRate(const ReportContext& context) const {
    report_detail::validatePeriod(period_, context);
    std::map<std::string, std::int64_t> purchases;
    for (const auto& [id, order] : context.orders.orders()) {
        if (!report_detail::deliveredIn(*order, period_)) {
            continue;
        }
        const auto returns = report_detail::returnedBefore(*order, period_.endsAt(), context);
        const bool retained =
            std::any_of(order->lines().begin(), order->lines().end(), [&returns](const auto& line) {
                const auto returned = returns.find(line.sku());
                return returned == returns.end() || returned->second.quantity < line.quantity();
            });
        if (retained) {
            purchases[order->customerId()] =
                detail::addNonnegative(purchases[order->customerId()], 1);
        }
    }
    if (purchases.empty()) {
        return std::nullopt;
    }
    const auto repeats = std::count_if(purchases.begin(), purchases.end(),
                                       [](const auto& pair) { return pair.second >= 2; });
    return ExactRatio{repeats, static_cast<std::int64_t>(purchases.size())};
}
std::map<std::string, RfmSegment> SalesReport::rfmSegments(const ReportContext& context,
                                                           const RfmRules& rules) const {
    report_detail::validatePeriod(period_, context);
    detail::check<ReportRuleException>(
        rules.recentSeconds >= 0 && rules.olderSeconds > rules.recentSeconds &&
            rules.mediumFrequency > 0 && rules.highFrequency > rules.mediumFrequency &&
            rules.mediumSpend >= Money{} && rules.highSpend > rules.mediumSpend,
        "Некорректные пороги RFM");
    std::map<std::string, RfmSegment> result;
    for (const auto& [id, customer] : context.customers) {
        if (customer.registeredAt() >= period_.endsAt()) {
            continue;
        }
        RfmSegment segment;
        for (const auto& [orderId, order] : context.orders.orders()) {
            if (order->customerId() != id || !report_detail::deliveredIn(*order, period_)) {
                continue;
            }
            const auto recency = period_.endsAt() - *order->deliveredAt();
            segment.recencySeconds =
                segment.recencySeconds ? std::min(*segment.recencySeconds, recency) : recency;
            segment.frequency = detail::addNonnegative(segment.frequency, 1);
            segment.monetary = segment.monetary.checkedAdd(
                report_detail::netRetainedGoods(*order, period_.endsAt(), context));
        }
        if (!segment.recencySeconds) {
            segment.segment = "no-purchases";
        } else {
            const int recency = *segment.recencySeconds <= rules.recentSeconds  ? 3
                                : *segment.recencySeconds <= rules.olderSeconds ? 2
                                                                                : 1;
            const int frequency = segment.frequency >= rules.highFrequency     ? 3
                                  : segment.frequency >= rules.mediumFrequency ? 2
                                                                               : 1;
            const int monetary = segment.monetary >= rules.highSpend     ? 3
                                 : segment.monetary >= rules.mediumSpend ? 2
                                                                         : 1;
            segment.segment =
                std::to_string(recency) + std::to_string(frequency) + std::to_string(monetary);
        }
        result.emplace(id, std::move(segment));
    }
    return result;
}
std::map<std::pair<DiscountKind, std::string>, PromotionStatistics>
SalesReport::promotionUsageAnalysis(const ReportContext& context) const {
    report_detail::validatePeriod(period_, context);
    std::map<std::pair<DiscountKind, std::string>, PromotionStatistics> result;
    for (const auto& [id, order] : context.orders.orders()) {
        if (!period_.contains(order->createdAt())) {
            continue;
        }
        const auto& source = order->discountSource();
        if (source.kind != DiscountKind::Promotion && source.kind != DiscountKind::Coupon) {
            continue;
        }
        auto& entry = result[{source.kind, source.id}];
        entry.orders = detail::addNonnegative(entry.orders, 1);
        const auto historical = order->stateBefore(period_.endsAt());
        const bool paid = std::any_of(order->history().begin(), order->history().end(),
                                      [this](const auto& event) {
                                          return event.kind() == OrderEventKind::PaymentConfirmed &&
                                                 event.timestamp() < period_.endsAt();
                                      });
        entry.paidOrders = detail::addNonnegative(entry.paidOrders, paid);
        const bool held = source.couponCode && historical.status == OrderStatus::AwaitingPayment &&
                          historical.paymentDeadline >= period_.endsAt();
        entry.heldCoupons = detail::addNonnegative(entry.heldCoupons, held);
        for (const auto& line : order->lines()) {
            entry.goodsDiscount = entry.goodsDiscount.checkedAdd(line.allocation().discountTotal());
        }
        for (const auto& [sku, fact] :
             report_detail::returnedBefore(*order, period_.endsAt(), context)) {
            entry.returnedGoods = entry.returnedGoods.checkedAdd(fact.amount);
        }
    }
    return result;
}
} // namespace shop
