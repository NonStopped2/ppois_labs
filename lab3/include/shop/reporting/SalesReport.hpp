#pragma once

#include "shop/customers/Customer.hpp"
#include "shop/orders/OrderBook.hpp"
#include "shop/payments/PaymentBook.hpp"
#include "shop/returns/ReturnInspection.hpp"
#include "shop/returns/ReturnRequest.hpp"
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace shop {
struct ReportContext {
    ReportContext(const OrderBook& orderValues, const PaymentBook& paymentValues,
                  const std::map<std::string, ReturnRequest>& requestValues,
                  const std::map<std::string, ReturnInspection>& inspectionValues,
                  const std::map<std::string, Customer>& customerValues, Timestamp currentTime)
        : orders(orderValues), payments(paymentValues), requests(requestValues),
          inspections(inspectionValues), customers(customerValues), now(currentTime) {}
    const OrderBook& orders;
    const PaymentBook& payments;
    const std::map<std::string, ReturnRequest>& requests;
    const std::map<std::string, ReturnInspection>& inspections;
    const std::map<std::string, Customer>& customers;
    Timestamp now;
};
struct ExactRatio {
    std::int64_t numerator = 0;
    std::int64_t denominator = 1;
    bool operator==(const ExactRatio&) const = default;
};
struct SkuStatistics {
    std::int64_t deliveredUnits = 0;
    std::int64_t returnedUnits = 0;
    Money deliveredGoods;
    Money returnedGoods;
    Money netGoods() const;
    bool operator==(const SkuStatistics&) const = default;
};
struct AbcEntry {
    std::string sku;
    Money netRevenue;
    char group = 'C';
    Money cumulativeRevenue;
};
struct QualityStatistics {
    std::int64_t claims = 0;
    std::int64_t receivedUnits = 0;
    std::int64_t acceptedDefectiveUnits = 0;
    std::int64_t restockUnits = 0;
    std::int64_t declinedUnits = 0;
    std::map<ReturnReason, std::int64_t> claimsByReason;
};
struct RfmRules {
    std::int64_t recentSeconds = 30 * 24 * 60 * 60;
    std::int64_t olderSeconds = 90 * 24 * 60 * 60;
    std::int64_t mediumFrequency = 2;
    std::int64_t highFrequency = 5;
    Money mediumSpend{100000};
    Money highSpend{500000};
};
struct RfmSegment {
    std::optional<std::int64_t> recencySeconds;
    std::int64_t frequency = 0;
    Money monetary;
    std::string segment;
};
struct PromotionStatistics {
    std::int64_t orders = 0;
    std::int64_t paidOrders = 0;
    std::int64_t heldCoupons = 0;
    Money goodsDiscount;
    Money returnedGoods;
};

/** SalesReport.
 * Строит денежные отчёты и показатели заказов по документам магазина. Денежные события и
 * когорты доставок используют разные даты.
 */
class SalesReport {
  public:
    static SalesReport buildFinancialSummary(const TimeWindow& period,
                                             const ReportContext& context);
    static std::map<std::string, SkuStatistics> buildSkuSummary(const TimeWindow& period,
                                                                const ReportContext& context);
    const TimeWindow& period() const noexcept;
    Money captured() const noexcept;
    Money refunded() const noexcept;
    Money deliveryRevenue() const noexcept;
    Money deliveredNetGoods() const noexcept;
    const std::map<std::string, SkuStatistics>& skuStatistics() const noexcept;
    const std::map<OrderStatus, std::int64_t>& orderStatusCounts() const noexcept;
    std::optional<ExactRatio> returnRate() const;
    std::optional<Money> averageOrderValue(const ReportContext& context) const;
    std::vector<std::string> reconcile(const ReportContext& context) const;
    std::vector<AbcEntry> abcAnalysis(std::int64_t aBoundaryBps = 8000,
                                      std::int64_t bBoundaryBps = 9500) const;
    std::map<std::string, QualityStatistics> qualityAnalysis(const ReportContext& context) const;
    std::optional<ExactRatio> repeatPurchaseRate(const ReportContext& context) const;
    std::map<std::string, RfmSegment> rfmSegments(const ReportContext& context,
                                                  const RfmRules& rules = {}) const;
    std::map<std::pair<DiscountKind, std::string>, PromotionStatistics>
    promotionUsageAnalysis(const ReportContext& context) const;

  private:
    explicit SalesReport(TimeWindow period);
    TimeWindow period_;
    Money captured_;
    Money refunded_;
    Money deliveryRevenue_;
    Money deliveredNetGoods_;
    std::map<std::string, SkuStatistics> skuStatistics_;
    std::map<OrderStatus, std::int64_t> orderStatusCounts_;
};
} // namespace shop
