#include "post/storage/StoragePolicy.hpp"
#include "post/detail/Validation.hpp"

#include <algorithm>
#include <limits>

namespace post {
namespace {
constexpr auto kSecondsPerDay =
    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::days{1}).count();
constexpr auto kSecondsPerHour =
    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::hours{1}).count();

// Вычитание безопасно после проверки неотрицательных упорядоченных меток.
std::chrono::seconds elapsed(std::chrono::sys_seconds start, std::chrono::sys_seconds end) {
    detail::requireTime(start);
    detail::requireTime(end);
    if (end < start) {
        throw std::invalid_argument("Observation precedes storage");
    }
    return end - start;
}
} // namespace
// Интервалы обязаны помещаться в секунды ещё до вычисления конкретных календарных дат.
StoragePolicy::StoragePolicy(const std::string& id, std::int64_t freeStorageDays,
                             std::int64_t maxStorageDays, Money dailyFee,
                             std::int64_t maxDeliveryAttempts, std::int64_t minRetryHours)
    : id_(detail::identifier(id)), freeStorageDays_(freeStorageDays),
      maxStorageDays_(maxStorageDays), dailyFee_(dailyFee),
      maxDeliveryAttempts_(maxDeliveryAttempts), minRetryHours_(minRetryHours) {
    const auto maximum = std::numeric_limits<std::chrono::seconds::rep>::max();
    if (freeStorageDays_ < 0 || maxStorageDays_ <= 0 || freeStorageDays_ > maxStorageDays_ ||
        maxDeliveryAttempts_ <= 0 || minRetryHours_ < 0 ||
        maxStorageDays_ > maximum / kSecondsPerDay || minRetryHours_ > maximum / kSecondsPerHour) {
        throw std::invalid_argument("Invalid storage policy limits");
    }
}
// Идентификатор отличает применённую политику от других настроек.
const std::string& StoragePolicy::id() const noexcept {
    return id_;
}
// Бесплатные дни не могут превышать общий срок хранения.
std::int64_t StoragePolicy::freeStorageDays() const noexcept {
    return freeStorageDays_;
}
// Общий срок ограничивает как начисление, так и возможность доставки.
std::int64_t StoragePolicy::maxStorageDays() const noexcept {
    return maxStorageDays_;
}
// Нулевая дневная цена означает бесплатное хранение в пределах срока.
const Money& StoragePolicy::dailyFee() const noexcept {
    return dailyFee_;
}
// Лимит включает первую попытку, а не только повторные.
std::int64_t StoragePolicy::maxDeliveryAttempts() const noexcept {
    return maxDeliveryAttempts_;
}
// Нулевой интервал явно разрешает повтор в тот же момент при оставшемся лимите.
std::int64_t StoragePolicy::minRetryHours() const noexcept {
    return minRetryHours_;
}
// Отдельно контролирует переполнение абсолютной временной метки.
std::chrono::sys_seconds StoragePolicy::deadlineFrom(std::chrono::sys_seconds storedAt) const {
    return detail::after(storedAt, std::chrono::seconds{maxStorageDays_ * kSecondsPerDay});
}
// Начисление прекращается на лимите, даже если наблюдение выполнено намного позже.
Money StoragePolicy::chargeUntil(std::chrono::sys_seconds storedAt,
                                 std::chrono::sys_seconds until) const {
    const auto span = std::min(elapsed(storedAt, until).count(), maxStorageDays_ * kSecondsPerDay);
    const auto paidSeconds = std::max<std::int64_t>(0, span - freeStorageDays_ * kSecondsPerDay);
    const auto paidDays = paidSeconds == 0 ? 0 : (paidSeconds - 1) / kSecondsPerDay + 1;
    return dailyFee_.multiply(paidDays);
}
// Проверяет согласованность сводки попыток, затем срок, количество и минимальный интервал.
bool StoragePolicy::canAttemptDelivery(std::chrono::sys_seconds storedAt,
                                       std::chrono::sys_seconds time, std::int64_t attemptsMade,
                                       std::optional<std::chrono::sys_seconds> lastAttempt) const {
    const auto age = elapsed(storedAt, time);
    if (attemptsMade < 0 || (attemptsMade == 0) != !lastAttempt.has_value()) {
        throw std::invalid_argument("Inconsistent attempt count and last attempt");
    }
    if (lastAttempt) {
        (void)elapsed(storedAt, *lastAttempt);
        (void)elapsed(*lastAttempt, time);
    }
    return age.count() < maxStorageDays_ * kSecondsPerDay && attemptsMade < maxDeliveryAttempts_ &&
           (!lastAttempt || (time - *lastAttempt).count() >= minRetryHours_ * kSecondsPerHour);
}

} // namespace post
