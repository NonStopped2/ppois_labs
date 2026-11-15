#include "shop/returns/ReturnInspection.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/ReturnRuleException.hpp"
#include "shop/common/Validation.hpp"
#include "shop/returns/ReturnRequest.hpp"

namespace shop {
ReturnInspection::ReturnInspection(std::string_view id, const ReturnRequest& request,
                                   Timestamp receivedAt)
    : id_(detail::text(id, "осмотр возврата")), requestId_(request.id()), receivedAt_(receivedAt) {
    detail::check<ReturnRuleException>(request.status() == ReturnStatus::Submitted &&
                                           receivedAt >= request.createdAt() &&
                                           receivedAt < request.deadline(),
                                       "Товар не получен в срок или заявка уже обработана");
}
const std::string& ReturnInspection::id() const noexcept {
    return id_;
}
const std::string& ReturnInspection::requestId() const noexcept {
    return requestId_;
}
Timestamp ReturnInspection::receivedAt() const noexcept {
    return receivedAt_;
}
const std::map<std::string, InspectionResult>& ReturnInspection::results() const noexcept {
    return results_;
}
bool ReturnInspection::finalized() const noexcept {
    return finalized_;
}
void ReturnInspection::recordResult(const InspectionResult& result, const ReturnRequest& request,
                                    const ReturnPolicy& policy) {
    detail::check<ReturnRuleException>(!finalized_ && request.id() == requestId_,
                                       "Осмотр завершён или относится к другой заявке");
    const auto requested = request.line(result.sku());
    result.assertConsistent();
    detail::check<ReturnRuleException>(
        result.receivedQuantity() <= requested.requestedQuantity() &&
            (result.restockQuantity() == 0 || policy.classifyRestockability(result.condition())),
        "Осмотр превышает запрос или нарушает правила пригодности");
    detail::check<DuplicateEntityException>(results_.emplace(result.sku(), result).second,
                                            "Повторная строка осмотра");
}
void ReturnInspection::finalize(const ReturnRequest& request) {
    detail::check<ReturnRuleException>(!finalized_ && request.id() == requestId_ &&
                                           results_.size() == request.lines().size(),
                                       "Неполный, чужой или уже завершённый осмотр");
    for (const auto& entry : request.lines()) {
        const auto& result = detail::get(results_, entry.sku());
        result.assertConsistent();
        detail::check<ReturnRuleException>(result.receivedQuantity() == entry.requestedQuantity(),
                                           "Требуется полное получение всех заявленных единиц");
    }
    finalized_ = true;
}
} // namespace shop
