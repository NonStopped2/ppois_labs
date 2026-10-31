#pragma once
#include "post/core/Money.hpp"

#include <chrono>
#include <optional>
#include <string>

namespace post {

// StoragePolicy — правила хранения и доставки: бесплатные дни, дедлайн, плата и лимиты попыток.
// По времени размещения вычисляет срок и плату; начавшиеся платные сутки округляет вверх.
// StorageEntry и DeliveryTask используют эту политику, но сами данные склада она не меняет.
class StoragePolicy {
public:
    StoragePolicy(const std::string& id, std::int64_t freeStorageDays, std::int64_t maxStorageDays,
                  Money dailyFee, std::int64_t maxDeliveryAttempts, std::int64_t minRetryHours);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] std::int64_t freeStorageDays() const noexcept;
    [[nodiscard]] std::int64_t maxStorageDays() const noexcept;
    [[nodiscard]] const Money& dailyFee() const noexcept;
    [[nodiscard]] std::int64_t maxDeliveryAttempts() const noexcept;
    [[nodiscard]] std::int64_t minRetryHours() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds deadlineFrom(std::chrono::sys_seconds storedAt) const;
    [[nodiscard]] Money chargeUntil(std::chrono::sys_seconds storedAt,
                                    std::chrono::sys_seconds until) const;
    [[nodiscard]] bool
    canAttemptDelivery(std::chrono::sys_seconds storedAt, std::chrono::sys_seconds time,
                       std::int64_t attemptsMade,
                       std::optional<std::chrono::sys_seconds> lastAttempt = std::nullopt) const;
    bool operator==(const StoragePolicy& other) const = default;

private:
    std::string id_;
    std::int64_t freeStorageDays_;
    std::int64_t maxStorageDays_;
    Money dailyFee_;
    std::int64_t maxDeliveryAttempts_;
    std::int64_t minRetryHours_;
};

} // namespace post
