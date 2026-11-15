#pragma once

#include "shop/application/StoreState.hpp"
#include "shop/common/SnapshotException.hpp"
#include <algorithm>
#include <iterator>
#include <limits>
#include <nlohmann/json.hpp>

namespace shop::snapshot {
using Json = nlohmann::json;
inline constexpr std::size_t MaximumSnapshotBytes = 16 * 1024 * 1024;
inline constexpr std::size_t MaximumCollectionSize = 100000;

void require(bool condition, std::string_view message);
std::int64_t integer(const Json& data);
bool boolean(const Json& data);
std::string string(const Json& data);
const Json& array(const Json& data);
const Json& object(const Json& data);
std::optional<std::string> optionalString(const Json& data);
Json nullable(const std::optional<std::string>& value);
std::set<std::string> stringSet(const Json& data);
std::map<std::string, std::string> stringMap(const Json& data);
std::map<std::string, std::int64_t> quantities(const Json& data);

template <class E> E enumeration(const Json& data, E maximum) {
    const auto value = integer(data);
    require(value >= 0 && value <= static_cast<std::int64_t>(maximum),
            "Неизвестное значение перечисления");
    return static_cast<E>(value);
}
template <class E> Json enumValue(E value) {
    return static_cast<std::int64_t>(value);
}
template <class Range, class Projection> Json arrayOf(const Range& range, Projection projection) {
    Json result = Json::array();
    for (const auto& entry : range) {
        result.push_back(projection(entry));
    }
    return result;
}
template <class T, class Decoder> std::vector<T> vectorOf(const Json& data, Decoder decoder) {
    const auto& entries = array(data);
    std::vector<T> result;
    std::transform(entries.begin(), entries.end(), std::back_inserter(result), decoder);
    return result;
}
template <class Map, class Value> void insertUnique(Map& map, const std::string& key, Value value) {
    require(map.emplace(key, std::move(value)).second, "Повторный ID в снимке: " + key);
}

Json value(const Address& address);
Address address(const Json& data);
Json value(const ContactDetails& contacts);
ContactDetails contacts(const Json& data);
Json value(const TimeWindow& window);
TimeWindow window(const Json& data);
Json value(const ProductAttribute& attribute);
ProductAttribute attribute(const Json& data);
Json value(const ProductVariant& variant);
ProductVariant variant(const Json& data);
Json value(const ReturnPolicy& policy);
ReturnPolicy returnsPolicy(const Json& data);
Json value(const DeliveryOption& option);
DeliveryOption deliveryOption(const Json& data);
Json value(const DeliveryPolicy& policy);
DeliveryPolicy deliveryPolicy(const Json& data);
Json value(const LoyaltyPolicy& policy);
LoyaltyPolicy loyaltyPolicy(const Json& data);
Json value(const PricingPolicy& policy);
PricingPolicy pricingPolicy(const Json& data);
Json value(const DiscountSource& source);
DiscountSource discountSource(const Json& data);
Json value(const QuoteLine& line);
QuoteLine quoteLine(const Json& data);
Json value(const PriceQuote& quote);
PriceQuote quote(const Json& data);
Json value(const Promotion& promotion);
Promotion promotion(const Json& data);
Json value(const Coupon& coupon);
Coupon coupon(const Json& data);
Json value(const InspectionResult& result);
InspectionResult inspectionResult(const Json& data);
Json value(const DeliveryTerms& terms);
DeliveryTerms deliveryTerms(const Json& data);
Json value(const OrderEvent& event);
OrderEvent event(const Json& data);
Json orderValue(const Order& item);
Order order(const Json& data);
Order paidPrefix(const Order& order);
Json outcomeValue(const AttemptOutcome& item);
AttemptOutcome outcome(const Json& data);
Json signatureValue(const CommandSignature& item);
CommandSignature signature(const Json& data);
Json resultValue(const OperationResult& item);
OperationResult operationResult(const Json& data);
Json paymentOperationValue(const PaymentOperation& item);
PaymentOperation paymentOperation(const Json& data);
Money requestAmount(const ReturnRequest& request, const ReturnInspection& inspection,
                    const Order& order);
void checkCounter(std::string_view id, std::string_view prefix, std::int64_t next);
} // namespace shop::snapshot
