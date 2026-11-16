#include "../ConsoleApplication.hpp"

namespace shop::ui {
void ConsoleApplication::reportCommand(Arguments& args) {
    requireOperator();
    const TimeWindow period{args.number("начало"), args.number("конец")};
    args.finish();
    const auto context = state_.reportContext();
    const auto report = SalesReport::buildFinancialSummary(period, context);
    output_ << "Отчёт [" << period.startsAt() << ',' << period.endsAt() << ")\n";
    output_ << "Оплачено: " << report.captured().format()
            << ", возвращено: " << report.refunded().format()
            << ", денежный поток: " << report.captured().checkedSubtract(report.refunded()).format()
            << '\n';
    output_ << "Доставка (денежные операции): " << report.deliveryRevenue().format()
            << ", чистые товары когорты получений: " << report.deliveredNetGoods().format() << '\n';
    if (const auto rate = report.returnRate()) {
        output_ << "Доля возврата единиц: " << rate->numerator << '/' << rate->denominator << '\n';
    } else {
        output_ << "Доля возврата: нет получений\n";
    }
    if (const auto average = report.averageOrderValue(context)) {
        output_ << "Средний товарный чек: " << average->format() << '\n';
    }
    for (const auto& [sku, stats] : report.skuStatistics()) {
        output_ << sku << " | получено " << stats.deliveredUnits << " | возвращено "
                << stats.returnedUnits << " | чистая сумма " << stats.netGoods().format() << '\n';
    }
    for (const auto& entry : report.abcAnalysis()) {
        output_ << "ABC " << entry.sku << ": " << entry.group << '\n';
    }
    for (const auto& [sku, quality] : report.qualityAnalysis(context)) {
        output_ << "Качество " << sku << " | обращений " << quality.claims << " | осмотрено "
                << quality.receivedUnits << " | принят брак " << quality.acceptedDefectiveUnits
                << " | пригодно " << quality.restockUnits << " | отклонено "
                << quality.declinedUnits << '\n';
    }
    if (const auto rate = report.repeatPurchaseRate(context)) {
        output_ << "Повторные покупатели: " << rate->numerator << '/' << rate->denominator << '\n';
    }
    for (const auto& [customer, segment] : report.rfmSegments(context)) {
        output_ << "RFM " << customer << ": " << segment.segment << " | получений "
                << segment.frequency << " | " << segment.monetary.format() << '\n';
    }
    for (const auto& [source, usage] : report.promotionUsageAnalysis(context)) {
        output_ << "Акция " << source.second << " | заказов " << usage.orders << " | оплачено "
                << usage.paidOrders << " | удержано " << usage.heldCoupons << " | скидки "
                << usage.goodsDiscount.format() << " | возвраты товаров "
                << usage.returnedGoods.format() << '\n';
    }
    const auto attempts = state_.payments().attemptOutcomeAnalysis(period);
    output_ << "Попытки: заказов " << attempts.orders << " | оплачено " << attempts.paidOrders
            << " | оплачено после отказа " << attempts.recoveredOrders << '\n';
    const auto issues = report.reconcile(context);
    output_ << "Сверка: " << issues.size() << " расхождений\n";
    for (const auto& issue : issues) {
        output_ << issue << '\n';
    }
}
} // namespace shop::ui
