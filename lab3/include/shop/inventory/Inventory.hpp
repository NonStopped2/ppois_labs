#pragma once
#include "shop/catalog/ProductCatalog.hpp"
#include "shop/inventory/StockItem.hpp"
#include "shop/inventory/StockReceipt.hpp"
#include "shop/inventory/StockReservation.hpp"
#include "shop/inventory/Warehouse.hpp"
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace shop {
/** Coordinates multi-SKU inventory operations with prepare-then-swap semantics. */
class Inventory {
  public:
    explicit Inventory(Warehouse warehouse);
    const Warehouse& warehouse() const noexcept;
    const std::map<std::string, StockItem>& items() const noexcept;
    const std::map<std::string, StockReservation>& reservations() const noexcept;
    const std::map<std::string, StockReceipt>& postedReceipts() const noexcept;
    StockItem item(std::string_view sku) const;
    StockReservation reservation(std::string_view id) const;
    std::int64_t available(std::string_view sku) const;
    void addStockItem(const ProductCatalog& catalog, std::string_view sku,
                      std::int64_t reorderPoint);
    void postReceipt(const StockReceipt& receipt, const ProductCatalog& catalog);
    void reserve(std::string_view id, std::string_view orderId,
                 const std::map<std::string, std::int64_t>& quantities, Timestamp now,
                 Timestamp deadline);
    void confirm(std::string_view id, Timestamp now);
    void extend(std::string_view id, Timestamp now, Timestamp deadline);
    void release(std::string_view id, Timestamp now);
    void expire(std::string_view id, Timestamp now);
    void consume(std::string_view id, Timestamp now);
    void restock(const std::map<std::string, std::int64_t>& quantities);
    void applyStockCount(std::string_view sku, std::int64_t actual);
    std::map<std::string, std::int64_t> replenishmentNeeds() const;
    std::vector<std::string> auditReservations() const;
    bool operator==(const Inventory&) const = default;

  private:
    void finish(std::string_view id, Timestamp now, ReservationStatus target);
    Warehouse warehouse_;
    std::map<std::string, StockItem> items_;
    std::map<std::string, StockReservation> reservations_;
    std::map<std::string, StockReceipt> postedReceipts_;
};
} // namespace shop
