#include "SnapshotJson.hpp"

namespace shop::snapshot {
void require(bool condition, std::string_view message) {
    if (!condition) {
        throw SnapshotException(std::string(message));
    }
}
std::int64_t integer(const Json& data) {
    require(data.is_number_integer(), "Ожидалось целое число JSON");
    require(!data.is_number_unsigned() ||
                data.get<std::uint64_t>() <=
                    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()),
            "Число JSON не помещается в int64");
    return data.get<std::int64_t>();
}
bool boolean(const Json& data) {
    require(data.is_boolean(), "Ожидалось логическое значение JSON");
    return data.get<bool>();
}
std::string string(const Json& data) {
    require(data.is_string(), "Ожидалась строка JSON");
    return data.get<std::string>();
}
const Json& array(const Json& data) {
    require(data.is_array() && data.size() <= MaximumCollectionSize,
            "Ожидался ограниченный массив JSON");
    return data;
}
const Json& object(const Json& data) {
    require(data.is_object() && data.size() <= MaximumCollectionSize,
            "Ожидался ограниченный объект JSON");
    return data;
}
std::optional<std::string> optionalString(const Json& data) {
    return data.is_null() ? std::nullopt : std::optional(string(data));
}
Json nullable(const std::optional<std::string>& item) {
    return item ? Json(*item) : Json(nullptr);
}
std::set<std::string> stringSet(const Json& data) {
    std::set<std::string> result;
    for (const auto& entry : array(data)) {
        require(result.insert(string(entry)).second, "Повторный элемент множества JSON");
    }
    return result;
}
std::map<std::string, std::string> stringMap(const Json& data) {
    std::map<std::string, std::string> result;
    for (const auto& [key, entry] : object(data).items()) {
        result.emplace(key, string(entry));
    }
    return result;
}
std::map<std::string, std::int64_t> quantities(const Json& data) {
    std::map<std::string, std::int64_t> result;
    for (const auto& [key, entry] : object(data).items()) {
        result.emplace(key, integer(entry));
    }
    return result;
}
Json value(const Address& item) {
    return {{"country", item.country()},   {"postal_code", item.postalCode()},
            {"city", item.city()},         {"street", item.street()},
            {"building", item.building()}, {"apartment", item.apartment()}};
}
Address address(const Json& data) {
    object(data);
    return Address(string(data.at("country")), string(data.at("postal_code")),
                   string(data.at("city")), string(data.at("street")), string(data.at("building")),
                   string(data.at("apartment")));
}
Json value(const ContactDetails& item) {
    return {{"name", item.fullName()}, {"email", item.email()}, {"phone", item.phone()}};
}
ContactDetails contacts(const Json& data) {
    object(data);
    return ContactDetails(string(data.at("name")), string(data.at("email")),
                          string(data.at("phone")));
}
Json value(const TimeWindow& item) {
    return Json::array({item.startsAt(), item.endsAt()});
}
TimeWindow window(const Json& data) {
    require(array(data).size() == 2, "Интервал JSON должен содержать две границы");
    return TimeWindow(integer(data[0]), integer(data[1]));
}
Json value(const ProductAttribute& item) {
    return {{"name", item.name()},
            {"kind", enumValue(item.kind())},
            {"required", item.required()},
            {"allowed", item.allowedValues()}};
}
ProductAttribute attribute(const Json& data) {
    object(data);
    return ProductAttribute(string(data.at("name")),
                            enumeration(data.at("kind"), AttributeKind::Integer),
                            boolean(data.at("required")), stringSet(data.at("allowed")));
}
Json value(const ProductVariant& item) {
    return {{"sku", item.sku()},
            {"label", item.label()},
            {"attributes", item.attributes()},
            {"weight_grams", item.weightGrams()},
            {"dimensions_mm", item.dimensionsMm()},
            {"sale_enabled", item.saleEnabled()}};
}
ProductVariant variant(const Json& data) {
    object(data);
    const auto& dimensions = array(data.at("dimensions_mm"));
    require(dimensions.size() == 3, "Требуются три габарита");
    return ProductVariant(string(data.at("sku")), string(data.at("label")),
                          stringMap(data.at("attributes")), integer(data.at("weight_grams")),
                          {integer(dimensions[0]), integer(dimensions[1]), integer(dimensions[2])},
                          boolean(data.at("sale_enabled")));
}
Json value(const ReturnPolicy& item) {
    return {{"window_seconds", item.returnWindow()},
            {"excluded_categories", item.nonReturnableCategories()},
            {"allowed_reasons",
             arrayOf(item.allowedReasons(), [](auto entry) { return enumValue(entry); })},
            {"restockable_conditions",
             arrayOf(item.restockableConditions(), [](auto entry) { return enumValue(entry); })}};
}
ReturnPolicy returnsPolicy(const Json& data) {
    object(data);
    std::set<ReturnReason> reasons;
    std::set<ItemCondition> conditions;
    for (const auto& entry : array(data.at("allowed_reasons"))) {
        require(reasons.insert(enumeration(entry, ReturnReason::WrongItem)).second,
                "Повторная причина возврата");
    }
    for (const auto& entry : array(data.at("restockable_conditions"))) {
        require(conditions.insert(enumeration(entry, ItemCondition::Incomplete)).second,
                "Повторное состояние товара");
    }
    return ReturnPolicy(integer(data.at("window_seconds")),
                        stringSet(data.at("excluded_categories")), std::move(reasons),
                        std::move(conditions));
}
Json value(const DeliveryOption& item) {
    return {{"id", item.id()},
            {"name", item.name()},
            {"postal_prefixes", item.postalPrefixes()},
            {"maximum_weight", item.maximumWeightGrams()},
            {"maximum_dimension", item.maximumDimensionMm()},
            {"base_fee", item.baseFee().kopecks()},
            {"free_from", item.freeFromSubtotal().kopecks()}};
}
DeliveryOption deliveryOption(const Json& data) {
    object(data);
    return DeliveryOption(
        string(data.at("id")), string(data.at("name")), stringSet(data.at("postal_prefixes")),
        integer(data.at("maximum_weight")), integer(data.at("maximum_dimension")),
        Money(integer(data.at("base_fee"))), Money(integer(data.at("free_from"))));
}
Json value(const DeliveryPolicy& item) {
    return {
        {"options", arrayOf(item.options(), [](const auto& pair) { return value(pair.second); })},
        {"handling_seconds", item.handlingDuration()},
        {"transit_seconds", item.transitDurations()}};
}
DeliveryPolicy deliveryPolicy(const Json& data) {
    object(data);
    const auto choices = vectorOf<DeliveryOption>(data.at("options"), deliveryOption);
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> durations;
    for (const auto& [key, entry] : object(data.at("transit_seconds")).items()) {
        require(array(entry).size() == 2, "Неверный интервал доставки");
        durations.emplace(key, std::make_pair(integer(entry[0]), integer(entry[1])));
    }
    return DeliveryPolicy(choices, integer(data.at("handling_seconds")), std::move(durations));
}
Json value(const LoyaltyPolicy& item) {
    return {{"thresholds", arrayOf(item.thresholds(), [](Money entry) { return entry.kopecks(); })},
            {"discounts_bps", item.discounts()},
            {"maximum_bps", item.maximumDiscountBps()}};
}
LoyaltyPolicy loyaltyPolicy(const Json& data) {
    object(data);
    auto thresholds = vectorOf<Money>(data.at("thresholds"),
                                      [](const auto& entry) { return Money(integer(entry)); });
    auto discounts = vectorOf<std::int64_t>(data.at("discounts_bps"), integer);
    return LoyaltyPolicy(std::move(thresholds), std::move(discounts),
                         integer(data.at("maximum_bps")));
}
Json value(const PricingPolicy& item) {
    return {{"quote_seconds", item.quoteLifetime()},
            {"maximum_cart_units", item.maximumCartUnits()},
            {"prefer_loyalty", item.preferLoyaltyOnTie()}};
}
PricingPolicy pricingPolicy(const Json& data) {
    object(data);
    return PricingPolicy(integer(data.at("quote_seconds")), integer(data.at("maximum_cart_units")),
                         boolean(data.at("prefer_loyalty")));
}
Json value(const DiscountSource& item) {
    return {{"kind", enumValue(item.kind)}, {"id", item.id}, {"coupon", nullable(item.couponCode)}};
}
DiscountSource discountSource(const Json& data) {
    object(data);
    return {enumeration(data.at("kind"), DiscountKind::Loyalty), string(data.at("id")),
            optionalString(data.at("coupon"))};
}
Json value(const QuoteLine& item) {
    return {{"sku", item.sku()},
            {"title", item.title()},
            {"quantity", item.quantity()},
            {"unit_price", item.unitPrice().kopecks()},
            {"discount", item.allocation().discountTotal().kopecks()},
            {"net_total", item.netTotal().kopecks()},
            {"weight_grams", item.packageSnapshot().weightGrams},
            {"dimensions_mm", item.packageSnapshot().dimensionsMm},
            {"attributes", item.packageSnapshot().attributes},
            {"return_eligible", item.returnEligible()}};
}
QuoteLine quoteLine(const Json& data) {
    object(data);
    const auto& dims = array(data.at("dimensions_mm"));
    require(dims.size() == 3, "Требуются три габарита позиции");
    const ProductVariant saved(string(data.at("sku")), string(data.at("title")),
                               stringMap(data.at("attributes")), integer(data.at("weight_grams")),
                               {integer(dims[0]), integer(dims[1]), integer(dims[2])});
    QuoteLine result(saved, string(data.at("title")), integer(data.at("quantity")),
                     Money(integer(data.at("unit_price"))), Money(integer(data.at("discount"))),
                     boolean(data.at("return_eligible")));
    require(result.netTotal() == Money(integer(data.at("net_total"))),
            "Чистая стоимость позиции не согласована со скидкой");
    return result;
}
Json value(const PriceQuote& item) {
    return {{"customer_id", item.customerId()},
            {"lines", arrayOf(item.lines(), [](const auto& line) { return value(line); })},
            {"address", value(item.deliveryAddress())},
            {"delivery_option", item.deliveryOptionId()},
            {"delivery_fee", item.deliveryFee().kopecks()},
            {"discount_source", value(item.discountSource())},
            {"created_at", item.createdAt()},
            {"valid_until", item.validUntil()},
            {"total", item.total().kopecks()},
            {"return_policy", value(item.returnPolicy())}};
}
PriceQuote quote(const Json& data) {
    object(data);
    auto lines = vectorOf<QuoteLine>(data.at("lines"), quoteLine);
    PriceQuote result(string(data.at("customer_id")), std::move(lines), address(data.at("address")),
                      string(data.at("delivery_option")), Money(integer(data.at("delivery_fee"))),
                      discountSource(data.at("discount_source")), integer(data.at("created_at")),
                      integer(data.at("valid_until")), returnsPolicy(data.at("return_policy")));
    require(result.total() == Money(integer(data.at("total"))),
            "Итог предложения не согласован с позициями");
    return result;
}
Json value(const Promotion& item) {
    return {{"id", item.id()},
            {"name", item.name()},
            {"kind", enumValue(item.kind())},
            {"value", item.value()},
            {"minimum_subtotal", item.minimumSubtotal().kopecks()},
            {"window", value(item.window())},
            {"categories", item.categoryIds()},
            {"automatic", item.automatic()},
            {"enabled", item.enabled()}};
}
Promotion promotion(const Json& data) {
    object(data);
    return Promotion(string(data.at("id")), string(data.at("name")),
                     enumeration(data.at("kind"), PromotionKind::FixedAmount),
                     integer(data.at("value")), Money(integer(data.at("minimum_subtotal"))),
                     window(data.at("window")), stringSet(data.at("categories")),
                     boolean(data.at("automatic")), boolean(data.at("enabled")));
}
Json value(const Coupon& item) {
    return {{"code", item.code()},
            {"promotion_id", item.promotionId()},
            {"total_limit", item.totalUseLimit()},
            {"customer_limit", item.perCustomerLimit()},
            {"window", value(item.window())},
            {"enabled", item.enabled()}};
}
Coupon coupon(const Json& data) {
    object(data);
    return Coupon(string(data.at("code")), string(data.at("promotion_id")),
                  integer(data.at("total_limit")), integer(data.at("customer_limit")),
                  window(data.at("window")), boolean(data.at("enabled")));
}
Json value(const InspectionResult& item) {
    return {{"sku", item.sku()},
            {"received", item.receivedQuantity()},
            {"accepted", item.acceptedQuantity()},
            {"restock", item.restockQuantity()},
            {"condition", enumValue(item.condition())}};
}
InspectionResult inspectionResult(const Json& data) {
    object(data);
    return InspectionResult(string(data.at("sku")), integer(data.at("received")),
                            integer(data.at("accepted")), integer(data.at("restock")),
                            enumeration(data.at("condition"), ItemCondition::Incomplete));
}
Json value(const DeliveryTerms& item) {
    return {{"address", value(item.address)},
            {"option_id", item.optionId},
            {"fee", item.fee.kopecks()}};
}
DeliveryTerms deliveryTerms(const Json& data) {
    object(data);
    return {address(data.at("address")), string(data.at("option_id")),
            Money(integer(data.at("fee")))};
}
} // namespace shop::snapshot
