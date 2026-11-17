#include "shop/inventory/StockReceipt.hpp"
#include "shop/common/Validation.hpp"
#include <map>
#include <numeric>
namespace shop {
StockReceipt::StockReceipt(std::string_view id, std::string_view supplierReference,
                           std::string_view warehouseId)
    : id_(detail::text(id, "приход")),
      supplierReference_(detail::text(supplierReference, "поставщик")),
      warehouseId_(detail::text(warehouseId, "склад")) {}
const std::string& StockReceipt::id() const noexcept {
    return id_;
}
const std::string& StockReceipt::supplierReference() const noexcept {
    return supplierReference_;
}
const std::string& StockReceipt::warehouseId() const noexcept {
    return warehouseId_;
}
const std::vector<ReceiptLine>& StockReceipt::lines() const noexcept {
    return lines_;
}
ReceiptStatus StockReceipt::status() const noexcept {
    return status_;
}
void StockReceipt::addLine(const ReceiptLine& line) {
    detail::require(status_ == ReceiptStatus::Draft, "Проведённый приход неизменяем");
    lines_.push_back(line);
}
void StockReceipt::mergeDuplicateSku() {
    detail::require(status_ == ReceiptStatus::Draft, "Проведённый приход неизменяем");
    std::map<std::pair<std::string, std::string>, ReceiptLine> merged;
    for (const auto& line : lines_) {
        const auto key = std::make_pair(line.sku(), line.batchReference());
        const auto found = merged.find(key);
        if (found == merged.end()) {
            merged.emplace(key, line);
            continue;
        }
        detail::require(found->second.purchaseUnitCost() == line.purchaseUnitCost(),
                        "Разные закупочные цены одной партии");
        found->second = ReceiptLine(
            line.sku(), detail::addNonnegative(found->second.quantity(), line.quantity()),
            line.purchaseUnitCost(), line.batchReference());
    }
    std::vector<ReceiptLine> result;
    for (const auto& [key, line] : merged) {
        result.push_back(line);
    }
    lines_.swap(result);
}
Money StockReceipt::validateForPosting() const {
    detail::require(status_ == ReceiptStatus::Draft && !lines_.empty(),
                    "Требуется непустой черновик прихода");
    return std::accumulate(lines_.begin(), lines_.end(), Money{}, [](Money sum, const auto& line) {
        return sum.checkedAdd(line.total());
    });
}
bool StockReceipt::sameDocument(const StockReceipt& other) const {
    return id_ == other.id_ && supplierReference_ == other.supplierReference_ &&
           warehouseId_ == other.warehouseId_ && lines_ == other.lines_;
}
} // namespace shop
