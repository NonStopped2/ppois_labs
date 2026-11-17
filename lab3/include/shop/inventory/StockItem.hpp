#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace shop {
/** StockItem.
 * Хранит физический и зарезервированный остаток одного SKU. Резерв всегда находится в
 * пределах физического остатка.
 */
class StockItem {
  public:
    explicit StockItem(std::string_view sku, std::int64_t onHand = 0, std::int64_t reserved = 0,
                       std::int64_t reorderPoint = 0);
    const std::string& sku() const noexcept;
    std::int64_t onHand() const noexcept;
    std::int64_t reserved() const noexcept;
    std::int64_t reorderPoint() const noexcept;
    std::int64_t available() const noexcept;
    void receive(std::int64_t quantity);
    void hold(std::int64_t quantity);
    void release(std::int64_t quantity);
    void consumeHeld(std::int64_t quantity);
    void applyCount(std::int64_t actual);
    bool operator==(const StockItem&) const = default;

  private:
    std::string sku_;
    std::int64_t onHand_;
    std::int64_t reserved_;
    std::int64_t reorderPoint_;
};
} // namespace shop
