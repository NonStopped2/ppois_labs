#include "shop/inventory/Inventory.hpp"
#include "shop/common/DuplicateEntityException.hpp"
#include "shop/common/DuplicateOperationException.hpp"
#include "shop/common/Lookup.hpp"
#include "shop/common/Validation.hpp"
#include <algorithm>

namespace shop {
Inventory::Inventory(Warehouse warehouse) : warehouse_(std::move(warehouse)) {}
const Warehouse& Inventory::warehouse() const noexcept {
    return warehouse_;
}
const std::map<std::string, StockItem>& Inventory::items() const noexcept {
    return items_;
}
const std::map<std::string, StockReservation>& Inventory::reservations() const noexcept {
    return reservations_;
}
const std::map<std::string, StockReceipt>& Inventory::postedReceipts() const noexcept {
    return postedReceipts_;
}
StockItem Inventory::item(std::string_view sku) const {
    return detail::get(items_, sku);
}
StockReservation Inventory::reservation(std::string_view id) const {
    return detail::get(reservations_, id);
}
std::int64_t Inventory::available(std::string_view sku) const {
    const auto found = items_.find(std::string(sku));
    return found == items_.end() ? 0 : found->second.available();
}
void Inventory::addStockItem(const ProductCatalog& catalog, std::string_view sku,
                             std::int64_t reorderPoint) {
    catalog.variant(sku);
    StockItem entry(sku, 0, 0, reorderPoint);
    detail::check<DuplicateEntityException>(items_.emplace(entry.sku(), entry).second,
                                            "Остаток SKU уже заведён");
}
void Inventory::postReceipt(const StockReceipt& receipt, const ProductCatalog& catalog) {
    const auto found = postedReceipts_.find(receipt.id());
    if (found != postedReceipts_.end()) {
        detail::check<DuplicateOperationException>(found->second.sameDocument(receipt),
                                                   "Приход с этим ID имеет другие данные");
        return;
    }
    warehouse_.assertMayReceive();
    detail::require(receipt.warehouseId() == warehouse_.id(), "Приход предназначен другому складу");
    receipt.validateForPosting();
    auto candidateItems = items_;
    for (const auto& line : receipt.lines()) {
        catalog.variant(line.sku());
        auto [entry, inserted] = candidateItems.try_emplace(line.sku(), line.sku());
        entry->second.receive(line.quantity());
    }
    auto documents = postedReceipts_;
    auto posted = receipt;
    posted.status_ = ReceiptStatus::Posted;
    documents.emplace(posted.id(), posted);
    items_.swap(candidateItems);
    postedReceipts_.swap(documents);
}
void Inventory::reserve(std::string_view id, std::string_view orderId,
                        const std::map<std::string, std::int64_t>& quantities, Timestamp now,
                        Timestamp deadline) {
    StockReservation entry(id, orderId, warehouse_.id(), quantities, now, deadline);
    detail::check<DuplicateEntityException>(
        !reservations_.contains(entry.id()) &&
            std::none_of(
                reservations_.begin(), reservations_.end(),
                [&entry](const auto& pair) { return pair.second.orderId() == entry.orderId(); }),
        "Повторный резерв или заказ");
    auto candidateItems = items_;
    for (const auto& [sku, quantity] : quantities) {
        detail::get(candidateItems, sku);
        candidateItems.at(sku).hold(quantity);
    }
    auto candidateReservations = reservations_;
    candidateReservations.emplace(entry.id(), entry);
    items_.swap(candidateItems);
    reservations_.swap(candidateReservations);
}
void Inventory::confirm(std::string_view id, Timestamp now) {
    const auto key = std::string(id);
    detail::get(reservations_, key);
    reservations_.at(key).confirm(now);
}
void Inventory::extend(std::string_view id, Timestamp now, Timestamp deadline) {
    const auto key = std::string(id);
    detail::get(reservations_, key);
    reservations_.at(key).extend(now, deadline);
}
void Inventory::finish(std::string_view id, Timestamp now, ReservationStatus target) {
    auto entry = reservation(id);
    if (target == ReservationStatus::Consumed) {
        entry.consume(now);
    } else if (target == ReservationStatus::Expired) {
        entry.expire(now);
    } else {
        entry.release(now);
    }
    auto candidateItems = items_;
    for (const auto& [sku, quantity] : entry.quantities()) {
        if (target == ReservationStatus::Consumed) {
            candidateItems.at(sku).consumeHeld(quantity);
        } else {
            candidateItems.at(sku).release(quantity);
        }
    }
    auto candidateReservations = reservations_;
    candidateReservations.at(entry.id()) = entry;
    items_.swap(candidateItems);
    reservations_.swap(candidateReservations);
}
void Inventory::release(std::string_view id, Timestamp now) {
    finish(id, now, ReservationStatus::Released);
}
void Inventory::expire(std::string_view id, Timestamp now) {
    finish(id, now, ReservationStatus::Expired);
}
void Inventory::consume(std::string_view id, Timestamp now) {
    finish(id, now, ReservationStatus::Consumed);
}
void Inventory::restock(const std::map<std::string, std::int64_t>& quantities) {
    auto candidate = items_;
    for (const auto& [sku, quantity] : quantities) {
        detail::get(candidate, sku);
        candidate.at(sku).receive(quantity);
    }
    items_.swap(candidate);
}
void Inventory::applyStockCount(std::string_view sku, std::int64_t actual) {
    const auto key = std::string(sku);
    detail::get(items_, key);
    items_.at(key).applyCount(actual);
}
std::map<std::string, std::int64_t> Inventory::replenishmentNeeds() const {
    std::map<std::string, std::int64_t> result;
    for (const auto& [sku, entry] : items_) {
        if (entry.available() < entry.reorderPoint()) {
            result.emplace(sku, entry.reorderPoint() - entry.available());
        }
    }
    return result;
}
std::vector<std::string> Inventory::auditReservations() const {
    std::map<std::string, std::int64_t> held;
    std::vector<std::string> issues;
    for (const auto& [id, entry] : reservations_) {
        if (!entry.live()) {
            continue;
        }
        for (const auto& [sku, quantity] : entry.quantities()) {
            held[sku] = detail::addNonnegative(held[sku], quantity);
        }
    }
    for (const auto& [sku, entry] : items_) {
        if (entry.reserved() != held[sku]) {
            issues.push_back("Несовпадение резерва: " + sku);
        }
        held.erase(sku);
    }
    for (const auto& [sku, quantity] : held) {
        issues.push_back("Резерв неизвестного остатка: " + sku);
    }
    return issues;
}
} // namespace shop
