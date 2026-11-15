#include "SnapshotCodec.hpp"
#include "SnapshotJson.hpp"

namespace shop {
using namespace snapshot;
Json SnapshotCodec::writeReturns(const StoreState& state) {
    return {{"requests", arrayOf(state.returnRequests_,
                                 [](const auto& pair) {
                                     const auto& item = pair.second;
                                     return Json{
                                         {"id", item.id()},
                                         {"order_id", item.orderId()},
                                         {"customer_id", item.customerId()},
                                         {"created_at", item.createdAt()},
                                         {"deadline", item.deadline()},
                                         {"status", enumValue(item.status())},
                                         {"inspection_id", nullable(item.inspectionId())},
                                         {"refund_id", nullable(item.refundId())},
                                         {"decision_reason", item.decisionReason()},
                                         {"lines", arrayOf(item.lines(), [](const auto& line) {
                                              return Json{{"sku", line.sku()},
                                                          {"quantity", line.requestedQuantity()},
                                                          {"offset", line.alreadyReturnedOffset()},
                                                          {"reason", enumValue(line.reason())},
                                                          {"maximum_refund",
                                                           line.maximumRefund().kopecks()}};
                                          })}};
                                 })},
            {"inspections", arrayOf(state.inspections_, [](const auto& pair) {
                 const auto& item = pair.second;
                 return Json{{"id", item.id()},
                             {"request_id", item.requestId()},
                             {"received_at", item.receivedAt()},
                             {"finalized", item.finalized()},
                             {"results", arrayOf(item.results(), [](const auto& entry) {
                                  return value(entry.second);
                              })}};
             })}};
}
void SnapshotCodec::readReturns(StoreState& state, const Json& data) {
    object(data);
    for (const auto& saved : array(data.at("requests"))) {
        const auto order = state.orders_.order(string(saved.at("order_id")));
        std::vector<ReturnLine> lines;
        for (const auto& entry : array(saved.at("lines"))) {
            ReturnLine line(order->line(string(entry.at("sku"))), integer(entry.at("quantity")),
                            integer(entry.at("offset")),
                            enumeration(entry.at("reason"), ReturnReason::WrongItem));
            require(line.maximumRefund() == Money(integer(entry.at("maximum_refund"))),
                    "Максимальная сумма заявки не совпадает с покупкой");
            lines.push_back(line);
        }
        ReturnRequest request(string(saved.at("id")), *order, std::move(lines),
                              integer(saved.at("created_at")));
        require(request.customerId() == string(saved.at("customer_id")) &&
                    request.deadline() == integer(saved.at("deadline")),
                "Владелец или срок заявки не совпадает с покупкой");
        insertUnique(state.returnRequests_, request.id(), request);
    }
    for (const auto& saved : array(data.at("inspections"))) {
        const auto found = state.returnRequests_.find(string(saved.at("request_id")));
        require(found != state.returnRequests_.end(), "Осмотр неизвестной заявки");
        const auto& request = found->second;
        const auto order = state.orders_.order(request.orderId());
        ReturnInspection item(string(saved.at("id")), request, integer(saved.at("received_at")));
        for (const auto& entry : array(saved.at("results"))) {
            item.recordResult(inspectionResult(entry), request, order->returnPolicy());
        }
        require(boolean(saved.at("finalized")), "Незавершённый осмотр в реестре");
        item.finalize(request);
        insertUnique(state.inspections_, item.id(), item);
    }
    for (const auto& saved : array(data.at("requests"))) {
        auto& item = state.returnRequests_.at(string(saved.at("id")));
        const auto inspectionId = optionalString(saved.at("inspection_id"));
        if (inspectionId) {
            const auto found = state.inspections_.find(*inspectionId);
            require(found != state.inspections_.end(), "Нет осмотра заявки");
            item.attachInspection(found->second);
        }
        const auto status = enumeration(saved.at("status"), ReturnStatus::Withdrawn);
        if (status == ReturnStatus::Completed) {
            item.complete(optionalString(saved.at("refund_id")));
        } else {
            require(saved.at("refund_id").is_null(), "Денежный документ у незавершённой заявки");
        }
        if (status == ReturnStatus::Withdrawn) {
            item.withdraw(item.customerId(), item.createdAt());
        }
        if (status == ReturnStatus::Rejected) {
            item.reject(string(saved.at("decision_reason")), state.now());
        } else {
            require(string(saved.at("decision_reason")).empty(), "Причина отказа без отказа");
        }
        require(item.status() == status, "Состояние заявки не совпадает с документами");
    }
}
ReviewBook SnapshotCodec::readReviews(const Json& data, const StoreState& state) {
    object(data);
    ReviewBook result;
    for (const auto& saved : array(data.at("entries"))) {
        const auto rating = integer(saved.at("rating"));
        require(rating >= 1 && rating <= 5, "Некорректная оценка отзыва");
        ProductReview item(string(saved.at("id")), string(saved.at("customer_id")),
                           string(saved.at("product_id")), string(saved.at("order_id")),
                           static_cast<int>(rating), string(saved.at("text")));
        const auto& moderation = object(saved.at("moderation"));
        const auto status = enumeration(moderation.at("visibility"), ReviewVisibility::Hidden);
        const auto voters = stringSet(saved.at("helpful_voters"));
        require(voters.empty() || status != ReviewVisibility::Pending,
                "Голоса у непроверенного отзыва");
        item.publish();
        for (const auto& id : voters) {
            item.voteHelpful(state.customer(id));
        }
        if (status == ReviewVisibility::Hidden) {
            item.hide(string(moderation.at("reason")));
        } else {
            require(string(moderation.at("reason")).empty(), "Причина скрытия без скрытия");
        }
        if (status == ReviewVisibility::Pending) {
            item.edit(item.customerId(), item.rating(), item.text());
        }
        insertUnique(result.reviews_, item.id(), item);
        require(result.purchaseReviewIndex_
                    .emplace(std::make_pair(item.customerId(), item.productId()), item.id())
                    .second,
                "Повторный отзыв покупателя на продукт");
    }
    result.revision_ = integer(data.at("revision"));
    require(result.revision_ >= static_cast<std::int64_t>(result.reviews_.size()),
            "Некорректная ревизия отзывов");
    return result;
}
} // namespace shop
