#pragma once

#include "shop/application/StoreState.hpp"
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>

namespace shop {
/** SnapshotCodec.
 * Преобразует состояние в JSON и восстанавливает его со сверкой типов, ссылок и
 * инвариантов.
 */
class SnapshotCodec {
  public:
    static std::string encode(const StoreState& state);
    static StoreState decode(std::string_view document);

  private:
    using Json = nlohmann::json;
    static Json writeState(const StoreState& state);
    static Json writeCustomers(const StoreState& state);
    static Json writeInventory(const Inventory& inventory);
    static Json writePayments(const PaymentBook& payments);
    static Json writeReturns(const StoreState& state);
    static StoreState readState(const Json& data);
    static ProductCatalog readCatalog(const Json& data);
    static PriceList readPrices(const Json& data);
    static Inventory readInventory(const Json& data);
    static CouponBook readCoupons(const Json& data);
    static void readCustomers(StoreState& state, const Json& data);
    static OrderBook readOrders(const Json& data);
    static PaymentBook readPayments(const Json& data);
    static void readFulfillments(StoreState& state, const Json& data);
    static void readReturns(StoreState& state, const Json& data);
    static ReviewBook readReviews(const Json& data, const StoreState& state);
    static void validate(const StoreState& state);
    static void validateCustomers(const StoreState& state);
    static void validateOrders(const StoreState& state);
    static void validatePayments(const StoreState& state);
    static void validateReturns(const StoreState& state);
    static void validateOperations(const StoreState& state);
};
} // namespace shop
