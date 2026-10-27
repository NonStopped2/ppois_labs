#include "post/personnel/WorkShift.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include <algorithm>

namespace post {
// Плановый интервал полуоткрытый [start, end), пустая смена недопустима.
WorkShift::WorkShift(const std::string& id, const std::string& employeeId,
                     const std::string& branchId, std::chrono::sys_seconds scheduledStart,
                     std::chrono::sys_seconds scheduledEnd)
    : id_(detail::identifier(id)), employeeId_(detail::identifier(employeeId)),
      branchId_(detail::identifier(branchId)), scheduledStart_(scheduledStart),
      scheduledEnd_(scheduledEnd) {
    detail::requireTime(scheduledStart_);
    detail::requireTime(scheduledEnd_);
    if (scheduledEnd_ <= scheduledStart_) {
        throw std::invalid_argument("Shift interval must be positive");
    }
}
// Идентификатор смены отделён от идентификатора сотрудника.
const std::string& WorkShift::id() const noexcept {
    return id_;
}
// Карточка сотрудника не копируется в смену и проверяется при её начале.
const std::string& WorkShift::employeeId() const noexcept {
    return employeeId_;
}
// Место работы сравнивается с карточкой и вызывающим отделением.
const std::string& WorkShift::branchId() const noexcept {
    return branchId_;
}
// Плановое начало включено.
std::chrono::sys_seconds WorkShift::scheduledStart() const noexcept {
    return scheduledStart_;
}
// Плановый конец исключён для начала и текущих полномочий.
std::chrono::sys_seconds WorkShift::scheduledEnd() const noexcept {
    return scheduledEnd_;
}
// Пока смена не начата, фактического начала нет.
std::optional<std::chrono::sys_seconds> WorkShift::actualStart() const noexcept {
    return actualStart_;
}
// Конец появляется только после успешного finish.
std::optional<std::chrono::sys_seconds> WorkShift::actualEnd() const noexcept {
    return actualEnd_;
}
// Внешний код не может создать пересекающиеся перерывы через геттер.
const std::vector<std::pair<std::chrono::sys_seconds, std::chrono::sys_seconds>>&
WorkShift::breaks() const noexcept {
    return breaks_;
}
// Состояние меняется вместе с фактическими датами.
ShiftState WorkShift::state() const noexcept {
    return state_;
}
// Начало проверяет личность, место, активность и плановый интервал до записи дат.
void WorkShift::start(const Employee& employee, const std::string& targetBranchId,
                      std::chrono::sys_seconds time) {
    detail::requireTime(time);
    if (state_ != ShiftState::Scheduled) {
        throw InvalidShipmentStateException("Shift already started");
    }
    if (employee.id() != employeeId_ || detail::identifier(targetBranchId) != branchId_) {
        throw DeliveryAuthorizationException("Wrong employee or branch for shift");
    }
    employee.authorize(BranchOperation::StartShift, branchId_);
    if (time < scheduledStart_ || time >= scheduledEnd_) {
        throw std::invalid_argument("Shift start outside schedule");
    }
    actualStart_ = time;
    state_ = ShiftState::Active;
}
// Законченные перерывы добавляются по порядку; касание границ допустимо, пересечение — нет.
void WorkShift::addBreak(std::chrono::sys_seconds from, std::chrono::sys_seconds to) {
    detail::requireTime(from);
    detail::requireTime(to);
    if (state_ != ShiftState::Active) {
        throw InvalidShipmentStateException("Shift is not active");
    }
    if (from < *actualStart_ || to <= from || to > scheduledEnd_ ||
        (!breaks_.empty() && from < breaks_.back().second)) {
        throw std::invalid_argument("Invalid break interval");
    }
    breaks_.emplace_back(from, to);
}
// Финиш не может предшествовать началу или уже записанному перерыву.
void WorkShift::finish(std::chrono::sys_seconds time) {
    detail::requireTime(time);
    if (state_ != ShiftState::Active) {
        throw InvalidShipmentStateException("Shift is not active");
    }
    if (time < *actualStart_ || time > scheduledEnd_ ||
        (!breaks_.empty() && time < breaks_.back().second)) {
        throw std::invalid_argument("Invalid shift finish");
    }
    actualEnd_ = time;
    state_ = ShiftState::Finished;
}
// Исторический запрос обрезает и смену, и перерывы моментом наблюдения.
std::chrono::seconds WorkShift::workedDuration(std::chrono::sys_seconds asOf) const {
    detail::requireTime(asOf);
    if (!actualStart_) {
        throw InvalidShipmentStateException("Shift has not started");
    }
    if (asOf < *actualStart_) {
        throw std::invalid_argument("Observation precedes shift start");
    }
    const auto end = std::min(asOf, actualEnd_.value_or(scheduledEnd_));
    auto duration = end - *actualStart_;
    for (const auto& [from, to] : breaks_) {
        if (from < end) {
            duration -= std::min(to, end) - from;
        }
    }
    return duration;
}
// Даже смены в разных отделениях конфликтуют, если назначены одному человеку.
bool WorkShift::overlaps(const WorkShift& other) const noexcept {
    return employeeId_ == other.employeeId_ && scheduledStart_ < other.scheduledEnd_ &&
           other.scheduledStart_ < scheduledEnd_;
}
// Полномочия требуют активную смену и время вне перерывов, не только попадание в расписание.
bool WorkShift::isWorkingAt(std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    return state_ == ShiftState::Active && time >= *actualStart_ && time < scheduledEnd_ &&
           std::none_of(breaks_.begin(), breaks_.end(), [time](const auto& span) {
               return time >= span.first && time < span.second;
           });
}
} // namespace post
