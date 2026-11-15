#include "post/delivery/DeliveryAttempt.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"

// DeliveryAttempt — факт одной попытки: ID, время, результат, причина неудачи и пояснение.
// Проверяет согласованность результата с причиной и возможность повтора после интервала.
// Хранится в DeliveryTask; общий лимит попыток и срок дополнительно проверяет StoragePolicy.
namespace post {
// Неудача требует известную причину и содержательное пояснение; успех не маскирует отказ.
DeliveryAttempt::DeliveryAttempt(const std::string& id, std::chrono::sys_seconds attemptedAt,
                                 DeliveryOutcome outcome, DeliveryFailureReason failureReason,
                                 const std::string& note)
    : id_(detail::identifier(id)), attemptedAt_(attemptedAt), outcome_(outcome),
      failureReason_(failureReason), note_(detail::normalizeText(note)) {
    detail::requireTime(attemptedAt_);
    validateOutcome();
}
// Идентификатор отличает повторные попытки одного задания.
const std::string& DeliveryAttempt::id() const noexcept {
    return id_;
}
// Системные часы не подменяют переданную дату.
std::chrono::sys_seconds DeliveryAttempt::attemptedAt() const noexcept {
    return attemptedAt_;
}
// Результат не изменяется после регистрации факта.
DeliveryOutcome DeliveryAttempt::outcome() const noexcept {
    return outcome_;
}
// None допустим только при успешном вручении.
DeliveryFailureReason DeliveryAttempt::failureReason() const noexcept {
    return failureReason_;
}
// Пояснение хранится нормализованным для аудита неудачи.
const std::string& DeliveryAttempt::note() const noexcept {
    return note_;
}
// Отдельная проверка полезна перед включением результата в будущее задание доставки.
void DeliveryAttempt::validateOutcome() const {
    const bool known =
        outcome_ == DeliveryOutcome::Delivered || outcome_ == DeliveryOutcome::Failed;
    const bool validReason = failureReason_ >= DeliveryFailureReason::None &&
                             failureReason_ <= DeliveryFailureReason::Damaged;
    if (!known || !validReason ||
        (outcome_ == DeliveryOutcome::Delivered) !=
            (failureReason_ == DeliveryFailureReason::None) ||
        (!note_.empty() && !detail::hasPrintableText(note_)) ||
        (outcome_ == DeliveryOutcome::Failed && note_.empty())) {
        throw InvalidShipmentDataException("Inconsistent delivery attempt outcome");
    }
}
// Сравнение разности безопаснее attemptedAt + interval на пределе календаря.
bool DeliveryAttempt::canRetryAfter(std::chrono::sys_seconds time,
                                    std::chrono::seconds minimumInterval) const {
    detail::requireTime(time);
    if (time < attemptedAt_ || minimumInterval.count() < 0) {
        throw std::invalid_argument("Invalid delivery retry interval");
    }
    const bool retryable = failureReason_ == DeliveryFailureReason::RecipientAbsent ||
                           failureReason_ == DeliveryFailureReason::AddressNotFound;
    return outcome_ == DeliveryOutcome::Failed && retryable &&
           time - attemptedAt_ >= minimumInterval;
}
} // namespace post
