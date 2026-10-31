#pragma once
#include "post/storage/StorageEntry.hpp"
#include <vector>

namespace post {
// StorageCell — складская ячейка с пределами массы, числа мест и размеров одного отправления.
// Владеет записями StorageEntry, не самими Shipment; place() проверяет лимиты до добавления.
// take() удаляет размещение и возвращает его снимок; используется внутри StorageArea.
class StorageCell {
public:
    StorageCell(const std::string& id, std::int64_t maxWeightGrams, std::int64_t maxItems,
                const Dimensions& limit);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] std::int64_t maxWeightGrams() const noexcept;
    [[nodiscard]] std::int64_t maxItems() const noexcept;
    [[nodiscard]] const Dimensions& limit() const noexcept;
    [[nodiscard]] const std::vector<StorageEntry>& entries() const noexcept;
    [[nodiscard]] std::int64_t totalWeightGrams() const;
    [[nodiscard]] bool contains(const std::string& shipmentId) const;
    void place(const Shipment& shipment, const StoragePolicy& policy,
               std::chrono::sys_seconds time);
    [[nodiscard]] StorageEntry take(const std::string& shipmentId);
    bool operator==(const StorageCell& other) const = default;

private:
    std::string id_;
    std::int64_t maxWeightGrams_;
    std::int64_t maxItems_;
    Dimensions limit_;
    std::vector<StorageEntry> entries_;
};
} // namespace post
