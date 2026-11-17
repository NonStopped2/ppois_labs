#include "shop/orders/OrderBook.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/OrderStateException.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {
void OrderBook::add(const Order& entry) {
    detail::check<DuplicateEntityException>(!orders_.contains(entry.id()), "Повторный заказ");
    auto candidate = orders_;
    auto index = customerOrderIndex_;
    candidate.emplace(entry.id(), std::make_shared<const Order>(entry));
    index[entry.customerId()].insert(entry.id());
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    orders_.swap(candidate);
    customerOrderIndex_.swap(index);
    revision_ = nextRevision;
}
// Публикуем новый неизменяемый снимок заказа. Старые выданные снимки ещё доступны читателям.
void OrderBook::replace(const Order& entry) {
    const auto previous = order(entry.id());
    detail::check<OrderStateException>(
        previous->customerId() == entry.customerId() && previous->lines() == entry.lines() &&
            previous->reservationId() == entry.reservationId() &&
            previous->discountSource() == entry.discountSource() &&
            previous->returnPolicy() == entry.returnPolicy() &&
            previous->history().size() <= entry.history().size() &&
            std::equal(previous->history().begin(), previous->history().end(),
                       entry.history().begin()),
        "Нельзя переписать зафиксированные условия или историю заказа");
    const auto nextRevision = detail::addNonnegative(revision_, 1);
    auto candidate = std::make_shared<const Order>(entry);
    orders_.at(entry.id()).swap(candidate);
    revision_ = nextRevision;
}
std::shared_ptr<const Order> OrderBook::order(std::string_view id) const {
    return detail::get(orders_, id);
}
const std::map<std::string, std::shared_ptr<const Order>>& OrderBook::orders() const noexcept {
    return orders_;
}
std::int64_t OrderBook::revision() const noexcept {
    return revision_;
}
std::vector<std::string> OrderBook::findOverdueUnpaid(Timestamp now) const {
    detail::require(now >= 0, "Некорректное время поиска заказов");
    std::vector<std::string> result;
    for (const auto& [id, entry] : orders_) {
        if (entry->status() == OrderStatus::AwaitingPayment && entry->paymentDeadline() <= now) {
            result.push_back(id);
        }
    }
    return result;
}
std::vector<std::shared_ptr<const Order>>
OrderBook::customerHistory(std::string_view customerId, std::optional<OrderStatus> selectedStatus,
                           std::optional<TimeWindow> period) const {
    std::vector<std::shared_ptr<const Order>> result;
    const auto found = customerOrderIndex_.find(std::string(customerId));
    if (found == customerOrderIndex_.end()) {
        return result;
    }
    for (const auto& id : found->second) {
        const auto entry = order(id);
        if ((!selectedStatus || *selectedStatus == entry->status()) &&
            (!period || period->contains(entry->createdAt()))) {
            result.push_back(entry);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left->createdAt() != right->createdAt() ? left->createdAt() > right->createdAt()
                                                       : left->id() < right->id();
    });
    return result;
}
std::vector<std::shared_ptr<const Order>>
OrderBook::selectForPeriod(const TimeWindow& period) const {
    std::vector<std::shared_ptr<const Order>> result;
    for (const auto& [id, entry] : orders_) {
        if (period.contains(entry->createdAt())) {
            result.push_back(entry);
        }
    }
    return result;
}
bool OrderBook::operator==(const OrderBook& other) const {
    return revision_ == other.revision_ && customerOrderIndex_ == other.customerOrderIndex_ &&
           orders_.size() == other.orders_.size() &&
           std::equal(orders_.begin(), orders_.end(), other.orders_.begin(),
                      [](const auto& left, const auto& right) {
                          return left.first == right.first && *left.second == *right.second;
                      });
}
} // namespace shop
