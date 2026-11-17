#pragma once
#include "shop/inventory/ReceiptLine.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace shop {
enum class ReceiptStatus { Draft, Posted };
/** StockReceipt.
 * Собирает строки поставки и проверяет документ перед проведением. Повтор сверяется с
 * содержимым документа.
 */
class StockReceipt {
  public:
    StockReceipt(std::string_view id, std::string_view supplierReference,
                 std::string_view warehouseId);
    const std::string& id() const noexcept;
    const std::string& supplierReference() const noexcept;
    const std::string& warehouseId() const noexcept;
    const std::vector<ReceiptLine>& lines() const noexcept;
    ReceiptStatus status() const noexcept;
    void addLine(const ReceiptLine& line);
    void mergeDuplicateSku();
    Money validateForPosting() const;
    bool sameDocument(const StockReceipt& other) const;
    bool operator==(const StockReceipt&) const = default;

  private:
    friend class SnapshotCodec;
    friend class Inventory;
    std::string id_;
    std::string supplierReference_;
    std::string warehouseId_;
    std::vector<ReceiptLine> lines_;
    ReceiptStatus status_ = ReceiptStatus::Draft;
};
} // namespace shop
