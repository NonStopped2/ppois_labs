#include "shop/returns/ReturnRequest.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/ReturnRuleException.hpp"
#include "shop/common/Validation.hpp"
#include "shop/returns/ReturnInspection.hpp"
#include <algorithm>
#include <numeric>
#include <set>

namespace shop {
ReturnRequest::ReturnRequest(std::string_view id, const Order& order, std::vector<ReturnLine> lines,
                             Timestamp createdAt)
    : id_(detail::text(id, "заявка возврата")), orderId_(order.id()),
      customerId_(order.customerId()), lines_(std::move(lines)), createdAt_(createdAt),
      deadline_(0) {
    validateLines(lines_, order, createdAt);
    deadline_ = order.returnPolicy().deadline(*order.deliveredAt());
}
void ReturnRequest::validateLines(const std::vector<ReturnLine>& lines, const Order& order,
                                  Timestamp now) {
    detail::check<ReturnRuleException>(order.status() == OrderStatus::Delivered &&
                                           order.deliveredAt().has_value() && !lines.empty(),
                                       "Возврат требует непустой заявки и полученного заказа");
    std::set<std::string> seen;
    for (const auto& entry : lines) {
        detail::check<DuplicateEntityException>(seen.insert(entry.sku()).second,
                                                "Повторный SKU возврата");
        const auto purchased = order.line(entry.sku());
        detail::check<ReturnRuleException>(entry == ReturnLine(purchased, entry.requestedQuantity(),
                                                               entry.alreadyReturnedOffset(),
                                                               entry.reason()),
                                           "Условия возвратной строки не совпадают с покупкой");
        order.returnPolicy().assertEligible(purchased.returnEligible(), *order.deliveredAt(),
                                            entry.reason(), now);
    }
}
const std::string& ReturnRequest::id() const noexcept {
    return id_;
}
const std::string& ReturnRequest::orderId() const noexcept {
    return orderId_;
}
const std::string& ReturnRequest::customerId() const noexcept {
    return customerId_;
}
const std::vector<ReturnLine>& ReturnRequest::lines() const noexcept {
    return lines_;
}
Timestamp ReturnRequest::createdAt() const noexcept {
    return createdAt_;
}
Timestamp ReturnRequest::deadline() const noexcept {
    return deadline_;
}
ReturnStatus ReturnRequest::status() const noexcept {
    return status_;
}
const std::optional<std::string>& ReturnRequest::inspectionId() const noexcept {
    return inspectionId_;
}
const std::optional<std::string>& ReturnRequest::refundId() const noexcept {
    return refundId_;
}
const std::string& ReturnRequest::decisionReason() const noexcept {
    return decisionReason_;
}
bool ReturnRequest::active() const noexcept {
    return status_ == ReturnStatus::Submitted || status_ == ReturnStatus::Inspected;
}
Money ReturnRequest::maximumRefund() const {
    return std::accumulate(lines_.begin(), lines_.end(), Money{}, [](Money sum, const auto& entry) {
        return sum.checkedAdd(entry.maximumRefund());
    });
}
ReturnLine ReturnRequest::line(std::string_view sku) const {
    const auto found = std::find_if(lines_.begin(), lines_.end(),
                                    [sku](const auto& entry) { return entry.sku() == sku; });
    detail::check<ReturnRuleException>(found != lines_.end(), "SKU отсутствует в заявке возврата");
    return *found;
}
void ReturnRequest::assertOwner(std::string_view actor) const {
    detail::check<ReturnRuleException>(actor == customerId_, "Чужая заявка возврата");
}
void ReturnRequest::attachInspection(const ReturnInspection& inspection) {
    detail::check<ReturnRuleException>(
        status_ == ReturnStatus::Submitted && inspection.requestId() == id_ &&
            inspection.finalized() && inspection.receivedAt() >= createdAt_ &&
            inspection.receivedAt() < deadline_,
        "Осмотр не соответствует живой заявке");
    std::optional<std::string> selected = inspection.id();
    inspectionId_.swap(selected);
    status_ = ReturnStatus::Inspected;
}
void ReturnRequest::amend(std::vector<ReturnLine> replacements, const Order& order, Timestamp now) {
    detail::check<ReturnRuleException>(status_ == ReturnStatus::Submitted &&
                                           order.id() == orderId_ && now >= createdAt_ &&
                                           now < deadline_,
                                       "Нельзя изменить осмотренную или истёкшую заявку");
    validateLines(replacements, order, now);
    lines_.swap(replacements);
}
void ReturnRequest::withdraw(std::string_view actor, Timestamp now) {
    assertOwner(actor);
    detail::check<ReturnRuleException>(status_ == ReturnStatus::Submitted && now >= createdAt_ &&
                                           now < deadline_,
                                       "Заявку уже нельзя отозвать");
    status_ = ReturnStatus::Withdrawn;
}
void ReturnRequest::reject(std::string_view reason, Timestamp now) {
    auto explanation = detail::text(reason, "причина отказа возврата");
    detail::check<ReturnRuleException>(active() && now >= createdAt_,
                                       "Заявка уже закрыта или время некорректно");
    decisionReason_.swap(explanation);
    status_ = ReturnStatus::Rejected;
}
void ReturnRequest::complete(std::optional<std::string> selectedRefund) {
    detail::check<ReturnRuleException>(status_ == ReturnStatus::Inspected,
                                       "Завершение требует осмотра");
    if (selectedRefund) {
        *selectedRefund = detail::text(*selectedRefund, "возврат денег");
    }
    refundId_.swap(selectedRefund);
    status_ = ReturnStatus::Completed;
}
} // namespace shop
