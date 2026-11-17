#include "SnapshotJson.hpp"

namespace shop::snapshot {
Json outcomeValue(const AttemptOutcome& item) {
    return {{"attempt_id", item.attemptId},
            {"payment_id", nullable(item.paymentId)},
            {"successful", item.successful}};
}
AttemptOutcome outcome(const Json& data) {
    object(data);
    return {string(data.at("attempt_id")), optionalString(data.at("payment_id")),
            boolean(data.at("successful"))};
}
Json signatureValue(const CommandSignature& item) {
    return {{"kind", enumValue(item.kind)},
            {"actor", item.actor},
            {"entity_id", item.entityId},
            {"texts", item.texts},
            {"numbers", item.numbers},
            {"delivery", item.delivery ? value(*item.delivery) : Json(nullptr)},
            {"quote", item.quote ? value(*item.quote) : Json(nullptr)},
            {"return_selections", arrayOf(item.returnSelections,
                                          [](const auto& entry) {
                                              return Json{{"sku", entry.sku},
                                                          {"quantity", entry.quantity},
                                                          {"reason", enumValue(entry.reason)}};
                                          })},
            {"inspection_results",
             arrayOf(item.inspectionResults, [](const auto& entry) { return value(entry); })}};
}
CommandSignature signature(const Json& data) {
    object(data);
    CommandSignature result;
    result.kind = enumeration(data.at("kind"), CommandKind::ExpireReturn);
    result.actor = string(data.at("actor"));
    result.entityId = string(data.at("entity_id"));
    result.texts = vectorOf<std::string>(data.at("texts"), string);
    result.numbers = vectorOf<std::int64_t>(data.at("numbers"), integer);
    if (!data.at("delivery").is_null()) {
        result.delivery = deliveryTerms(data.at("delivery"));
    }
    if (!data.at("quote").is_null()) {
        result.quote = quote(data.at("quote"));
    }
    for (const auto& entry : array(data.at("return_selections"))) {
        result.returnSelections.push_back(
            {string(entry.at("sku")), integer(entry.at("quantity")),
             enumeration(entry.at("reason"), ReturnReason::WrongItem)});
    }
    result.inspectionResults =
        vectorOf<InspectionResult>(data.at("inspection_results"), inspectionResult);
    return result;
}
Json resultValue(const OperationResult& item) {
    Json encoded = nullptr;
    if (const auto* text = std::get_if<std::string>(&item)) {
        encoded = *text;
    }
    if (const auto* attempt = std::get_if<AttemptOutcome>(&item)) {
        encoded = outcomeValue(*attempt);
    }
    if (const auto* money = std::get_if<Money>(&item)) {
        encoded = money->kopecks();
    }
    return {{"kind", item.index()}, {"value", encoded}};
}
OperationResult operationResult(const Json& data) {
    object(data);
    const auto kind = integer(data.at("kind"));
    const auto& saved = data.at("value");
    switch (kind) {
    case 0:
        require(saved.is_null(), "Данные у пустого результата команды");
        return std::monostate{};
    case 1: return string(saved);
    case 2: return outcome(saved);
    case 3: return Money(integer(saved));
    default: throw SnapshotException("Неизвестный тип результата команды");
    }
}
Json paymentOperationValue(const PaymentOperation& item) {
    if (const auto* attempt = std::get_if<AttemptOperation>(&item)) {
        return {{"kind", 0},
                {"order_id", attempt->orderId},
                {"amount", attempt->amount.kopecks()},
                {"method", enumValue(attempt->method)},
                {"accepted", attempt->accepted},
                {"failure_reason", attempt->failureReason},
                {"result", outcomeValue(attempt->result)}};
    }
    const auto& refund = std::get<RefundOperation>(item);
    return {{"kind", 1},
            {"payment_id", refund.paymentId},
            {"return_request_id", nullable(refund.returnRequestId)},
            {"amount", refund.amount.kopecks()},
            {"result_id", refund.resultId}};
}
PaymentOperation paymentOperation(const Json& data) {
    object(data);
    const auto kind = integer(data.at("kind"));
    if (kind == 0) {
        return AttemptOperation{string(data.at("order_id")),
                                Money(integer(data.at("amount"))),
                                enumeration(data.at("method"), PaymentMethod::SimulatedTransfer),
                                boolean(data.at("accepted")),
                                string(data.at("failure_reason")),
                                outcome(data.at("result"))};
    }
    require(kind == 1, "Неизвестный тип финансовой операции");
    return RefundOperation{string(data.at("payment_id")),
                           optionalString(data.at("return_request_id")),
                           Money(integer(data.at("amount"))), string(data.at("result_id"))};
}
} // namespace shop::snapshot
