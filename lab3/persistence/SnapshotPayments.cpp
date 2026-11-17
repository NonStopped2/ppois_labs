#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"

namespace shop {
using namespace snapshot;
Json SnapshotCodec::writePayments(const PaymentBook& payments) {
    return {
        {"attempts", arrayOf(payments.attempts(),
                             [](const auto& pair) {
                                 const auto& item = pair.second;
                                 return Json{{"id", item.id()},
                                             {"order_id", item.orderId()},
                                             {"amount", item.amount().kopecks()},
                                             {"method", enumValue(item.method())},
                                             {"created_at", item.createdAt()},
                                             {"status", enumValue(item.status())},
                                             {"failure_reason", item.failureReason()}};
                             })},
        {"payments", arrayOf(payments.payments(),
                             [](const auto& pair) {
                                 const auto& item = pair.second;
                                 return Json{{"id", item.id()},
                                             {"attempt_id", item.attemptId()},
                                             {"order_id", item.orderId()},
                                             {"captured_amount", item.capturedAmount().kopecks()},
                                             {"captured_at", item.capturedAt()},
                                             {"refund_ids", item.refundIds()}};
                             })},
        {"refunds", arrayOf(payments.refunds(),
                            [](const auto& pair) {
                                const auto& item = pair.second;
                                return Json{{"id", item.id()},
                                            {"operation_id", item.operationId()},
                                            {"payment_id", item.paymentId()},
                                            {"order_id", item.orderId()},
                                            {"return_request_id", nullable(item.returnRequestId())},
                                            {"amount", item.amount().kopecks()},
                                            {"created_at", item.createdAt()}};
                            })},
        {"operations", arrayOf(payments.operationIndex(), [](const auto& pair) {
             return Json{{"id", pair.first}, {"operation", paymentOperationValue(pair.second)}};
         })}};
}
PaymentBook SnapshotCodec::readPayments(const Json& data) {
    object(data);
    PaymentBook result;
    for (const auto& saved : array(data.at("attempts"))) {
        PaymentAttempt item(string(saved.at("id")), string(saved.at("order_id")),
                            Money(integer(saved.at("amount"))),
                            enumeration(saved.at("method"), PaymentMethod::SimulatedTransfer),
                            integer(saved.at("created_at")));
        const auto status = enumeration(saved.at("status"), AttemptStatus::Declined);
        require(status != AttemptStatus::Pending, "Незавершённая попытка в финансовом реестре");
        if (status == AttemptStatus::Succeeded) {
            require(string(saved.at("failure_reason")).empty(),
                    "Причина отказа у успешной попытки");
            item.succeed();
        } else {
            item.decline(string(saved.at("failure_reason")));
        }
        insertUnique(result.attempts_, item.id(), item);
    }
    for (const auto& saved : array(data.at("payments"))) {
        const auto found = result.attempts_.find(string(saved.at("attempt_id")));
        require(found != result.attempts_.end(), "Платёж ссылается на неизвестную попытку");
        Payment item(string(saved.at("id")), found->second);
        require(item.orderId() == string(saved.at("order_id")) &&
                    item.capturedAmount() == Money(integer(saved.at("captured_amount"))) &&
                    item.capturedAt() == integer(saved.at("captured_at")),
                "Данные оплаты не совпадают с попыткой");
        item.refundIds_ = stringSet(saved.at("refund_ids"));
        insertUnique(result.payments_, item.id(), item);
    }
    for (const auto& saved : array(data.at("refunds"))) {
        const Refund item(string(saved.at("id")), string(saved.at("operation_id")),
                          string(saved.at("payment_id")), string(saved.at("order_id")),
                          optionalString(saved.at("return_request_id")),
                          Money(integer(saved.at("amount"))), integer(saved.at("created_at")));
        insertUnique(result.refunds_, item.id(), item);
    }
    for (const auto& saved : array(data.at("operations"))) {
        insertUnique(result.operationIndex_, string(saved.at("id")),
                     paymentOperation(saved.at("operation")));
    }
    return result;
}
} // namespace shop
