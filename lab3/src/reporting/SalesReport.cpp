#include "shop/reporting/SalesReport.hpp"
#include "ReportFacts.hpp"
#include <numeric>

namespace shop {
Money SkuStatistics::netGoods() const {
    return deliveredGoods.checkedSubtract(returnedGoods);
}
SalesReport::SalesReport(TimeWindow period) : period_(period) {}
SalesReport SalesReport::buildFinancialSummary(const TimeWindow& period,
                                               const ReportContext& context) {
    report_detail::validatePeriod(period, context);
    SalesReport report(period);
    for (const auto& [id, payment] : context.payments.payments()) {
        if (!period.contains(payment.capturedAt())) {
            continue;
        }
        report.captured_ = report.captured_.checkedAdd(payment.capturedAmount());
        report.deliveryRevenue_ = report.deliveryRevenue_.checkedAdd(
            context.orders.order(payment.orderId())->deliveryFee());
    }
    for (const auto& [id, refund] : context.payments.refunds()) {
        if (!period.contains(refund.createdAt())) {
            continue;
        }
        report.refunded_ = report.refunded_.checkedAdd(refund.amount());
        if (!refund.returnRequestId()) {
            report.deliveryRevenue_ = report.deliveryRevenue_.checkedSubtract(
                context.orders.order(refund.orderId())->deliveryFee());
        }
    }
    report.skuStatistics_ = buildSkuSummary(period, context);
    report.deliveredNetGoods_ = std::accumulate(
        report.skuStatistics_.begin(), report.skuStatistics_.end(), Money{},
        [](Money sum, const auto& pair) { return sum.checkedAdd(pair.second.netGoods()); });
    for (const auto& [id, order] : context.orders.orders()) {
        if (!period.contains(order->createdAt())) {
            continue;
        }
        auto& count = report.orderStatusCounts_[order->stateBefore(period.endsAt()).status];
        count = detail::addNonnegative(count, 1);
    }
    return report;
}
std::map<std::string, SkuStatistics> SalesReport::buildSkuSummary(const TimeWindow& period,
                                                                  const ReportContext& context) {
    report_detail::validatePeriod(period, context);
    std::map<std::string, SkuStatistics> result;
    for (const auto& [id, order] : context.orders.orders()) {
        if (!report_detail::deliveredIn(*order, period)) {
            continue;
        }
        const auto returned = report_detail::returnedBefore(*order, period.endsAt(), context);
        for (const auto& entry : order->lines()) {
            auto& summary = result[entry.sku()];
            summary.deliveredUnits =
                detail::addNonnegative(summary.deliveredUnits, entry.quantity());
            summary.deliveredGoods = summary.deliveredGoods.checkedAdd(entry.netTotal());
            const auto found = returned.find(entry.sku());
            if (found != returned.end()) {
                summary.returnedUnits =
                    detail::addNonnegative(summary.returnedUnits, found->second.quantity);
                summary.returnedGoods = summary.returnedGoods.checkedAdd(found->second.amount);
            }
        }
    }
    return result;
}
const TimeWindow& SalesReport::period() const noexcept {
    return period_;
}
Money SalesReport::captured() const noexcept {
    return captured_;
}
Money SalesReport::refunded() const noexcept {
    return refunded_;
}
Money SalesReport::deliveryRevenue() const noexcept {
    return deliveryRevenue_;
}
Money SalesReport::deliveredNetGoods() const noexcept {
    return deliveredNetGoods_;
}
const std::map<std::string, SkuStatistics>& SalesReport::skuStatistics() const noexcept {
    return skuStatistics_;
}
const std::map<OrderStatus, std::int64_t>& SalesReport::orderStatusCounts() const noexcept {
    return orderStatusCounts_;
}
std::optional<ExactRatio> SalesReport::returnRate() const {
    std::int64_t delivered = 0;
    std::int64_t returned = 0;
    for (const auto& [sku, entry] : skuStatistics_) {
        delivered = detail::addNonnegative(delivered, entry.deliveredUnits);
        returned = detail::addNonnegative(returned, entry.returnedUnits);
    }
    return delivered == 0 ? std::nullopt : std::optional<ExactRatio>({returned, delivered});
}
std::optional<Money> SalesReport::averageOrderValue(const ReportContext& context) const {
    report_detail::validatePeriod(period_, context);
    std::int64_t count = 0;
    for (const auto& [id, order] : context.orders.orders()) {
        if (report_detail::deliveredIn(*order, period_)) {
            count = detail::addNonnegative(count, 1);
        }
    }
    return count == 0 ? std::nullopt
                      : std::optional<Money>(Money(deliveredNetGoods_.kopecks() / count));
}
} // namespace shop
