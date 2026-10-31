#pragma once
#include "post/delivery/DeliveryOutcome.hpp"
#include <chrono>
#include <string>

namespace post {
// DeliveryAttempt — факт одной попытки: ID, время, результат, причина неудачи и пояснение.
// Проверяет согласованность результата с причиной и возможность повтора после интервала.
// Хранится в DeliveryTask; общий лимит попыток и срок дополнительно проверяет StoragePolicy.
class DeliveryAttempt {
public:
    DeliveryAttempt(const std::string& id, std::chrono::sys_seconds attemptedAt,
                    DeliveryOutcome outcome,
                    DeliveryFailureReason failureReason = DeliveryFailureReason::None,
                    const std::string& note = "");
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds attemptedAt() const noexcept;
    [[nodiscard]] DeliveryOutcome outcome() const noexcept;
    [[nodiscard]] DeliveryFailureReason failureReason() const noexcept;
    [[nodiscard]] const std::string& note() const noexcept;
    void validateOutcome() const;
    [[nodiscard]] bool canRetryAfter(std::chrono::sys_seconds time,
                                     std::chrono::seconds minimumInterval) const;
    bool operator==(const DeliveryAttempt& other) const = default;

private:
    std::string id_;
    std::chrono::sys_seconds attemptedAt_;
    DeliveryOutcome outcome_;
    DeliveryFailureReason failureReason_;
    std::string note_;
};
} // namespace post
