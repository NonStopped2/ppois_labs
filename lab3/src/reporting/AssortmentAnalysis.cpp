#include "ReportFacts.hpp"
#include "shop/reporting/SalesReport.hpp"
#include <algorithm>

namespace shop {
std::vector<AbcEntry> SalesReport::abcAnalysis(std::int64_t aBoundaryBps,
                                               std::int64_t bBoundaryBps) const {
    detail::check<ReportRuleException>(aBoundaryBps > 0 && bBoundaryBps > aBoundaryBps &&
                                           bBoundaryBps <= 10000,
                                       "Некорректные границы ABC");
    std::vector<AbcEntry> result;
    for (const auto& [sku, entry] : skuStatistics_) {
        result.push_back({sku, entry.netGoods(), 'C', Money{}});
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.netRevenue != right.netRevenue ? left.netRevenue > right.netRevenue
                                                   : left.sku < right.sku;
    });
    // Compare cumulative/total with each boundary using exact division, not overflowing products.
    const auto ceiling = [this](std::int64_t bps) {
        const auto [whole, remainder] = deliveredNetGoods_.fractionParts(bps, 10000);
        return whole.checkedAdd(Money(remainder > 0));
    };
    const auto a = ceiling(aBoundaryBps);
    const auto b = ceiling(bBoundaryBps);
    Money cumulative;
    for (auto& entry : result) {
        if (entry.netRevenue > Money{}) {
            entry.group = cumulative < a ? 'A' : cumulative < b ? 'B' : 'C';
        }
        cumulative = cumulative.checkedAdd(entry.netRevenue);
        entry.cumulativeRevenue = cumulative;
    }
    return result;
}
std::map<std::string, QualityStatistics>
SalesReport::qualityAnalysis(const ReportContext& context) const {
    report_detail::validatePeriod(period_, context);
    std::map<std::string, QualityStatistics> result;
    for (const auto& [id, inspection] : context.inspections) {
        if (!inspection.finalized() || !period_.contains(inspection.receivedAt())) {
            continue;
        }
        const auto& request = detail::get(context.requests, inspection.requestId());
        for (const auto& [sku, entry] : inspection.results()) {
            auto& summary = result[sku];
            summary.claims = detail::addNonnegative(summary.claims, 1);
            summary.receivedUnits =
                detail::addNonnegative(summary.receivedUnits, entry.receivedQuantity());
            summary.acceptedDefectiveUnits = detail::addNonnegative(
                summary.acceptedDefectiveUnits,
                entry.condition() == ItemCondition::Defective ? entry.acceptedQuantity() : 0);
            summary.restockUnits =
                detail::addNonnegative(summary.restockUnits, entry.restockQuantity());
            summary.declinedUnits = detail::addNonnegative(
                summary.declinedUnits, entry.receivedQuantity() - entry.acceptedQuantity());
            const auto reason = request.line(sku).reason();
            summary.claimsByReason[reason] =
                detail::addNonnegative(summary.claimsByReason[reason], 1);
        }
    }
    return result;
}
} // namespace shop
