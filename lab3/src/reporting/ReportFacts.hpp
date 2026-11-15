#pragma once

#include "shop/common/Lookup.hpp"
#include "shop/common/ReportRuleException.hpp"
#include "shop/common/Validation.hpp"
#include "shop/reporting/SalesReport.hpp"

namespace shop::report_detail {
inline void validatePeriod(const TimeWindow& period, const ReportContext& context) {
    detail::check<ReportRuleException>(period.endsAt() <= context.now,
                                       "Граница отчёта лежит в будущем");
}
struct ReturnedFact {
    std::int64_t quantity = 0;
    Money amount;
};
inline std::map<std::string, ReturnedFact> returnedBefore(const Order& order, Timestamp end,
                                                          const ReportContext& context) {
    std::map<std::string, ReturnedFact> result;
    for (const auto& event : order.history()) {
        if (event.timestamp() >= end || event.kind() != OrderEventKind::ReturnCompleted) {
            continue;
        }
        const auto& request = detail::get(context.requests, event.relatedEntityId());
        detail::check<ReportRuleException>(request.status() == ReturnStatus::Completed &&
                                               request.orderId() == order.id() &&
                                               request.inspectionId().has_value(),
                                           "Событие возврата не соответствует завершённой заявке");
        const auto& inspection = detail::get(context.inspections, *request.inspectionId());
        detail::check<ReportRuleException>(inspection.finalized() &&
                                               inspection.requestId() == request.id(),
                                           "Осмотр не согласован с возвратом");
        for (const auto& entry : request.lines()) {
            const auto& inspected = detail::get(inspection.results(), entry.sku());
            const auto purchased = order.line(entry.sku());
            auto& fact = result[entry.sku()];
            detail::check<ReportRuleException>(fact.quantity == entry.alreadyReturnedOffset(),
                                               "Возвратные смещения не согласованы с историей");
            fact.quantity = detail::addNonnegative(fact.quantity, inspected.acceptedQuantity());
            fact.amount = fact.amount.checkedAdd(purchased.allocation().netForUnitRange(
                entry.alreadyReturnedOffset(), inspected.acceptedQuantity()));
            detail::check<ReportRuleException>(fact.quantity <= purchased.quantity(),
                                               "Возвращено больше полученного");
        }
    }
    return result;
}
inline bool deliveredIn(const Order& order, const TimeWindow& period) {
    const auto time = order.deliveredAt();
    return time && period.contains(*time);
}
inline Money netRetainedGoods(const Order& order, Timestamp end, const ReportContext& context) {
    Money returned;
    for (const auto& [sku, fact] : returnedBefore(order, end, context)) {
        returned = returned.checkedAdd(fact.amount);
    }
    return order.goodsNet().checkedSubtract(returned);
}
} // namespace shop::report_detail
