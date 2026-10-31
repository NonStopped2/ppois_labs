#pragma once
#include "post/shipments/Shipment.hpp"
#include "post/storage/StoragePolicy.hpp"

namespace post {
// StorageEntry — запись размещения в ячейке: трек-номер, масса, размеры, приём и дедлайн.
// Не владеет Shipment, а сохраняет снимок данных для учёта вместимости и срока хранения.
// Создаётся только для доступного к хранению груза; по StoragePolicy считает плату.
class StorageEntry {
public:
    StorageEntry(const Shipment& shipment, const StoragePolicy& policy,
                 std::chrono::sys_seconds receivedAt, const std::string& cellId);
    [[nodiscard]] const std::string& shipmentId() const noexcept;
    [[nodiscard]] std::int64_t storedWeightGrams() const noexcept;
    [[nodiscard]] const Dimensions& dimensions() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds receivedAt() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds deadlineAt() const noexcept;
    [[nodiscard]] const std::string& cellId() const noexcept;
    [[nodiscard]] bool isOverdueAt(std::chrono::sys_seconds time) const;
    [[nodiscard]] Money storageFeeUntil(const StoragePolicy& policy,
                                        std::chrono::sys_seconds time) const;
    bool operator==(const StorageEntry& other) const = default;

private:
    std::string shipmentId_;
    std::int64_t storedWeightGrams_;
    Dimensions dimensions_;
    std::chrono::sys_seconds receivedAt_;
    std::chrono::sys_seconds deadlineAt_;
    std::string cellId_;
};
} // namespace post
