#pragma once
#include "post/personnel/Employee.hpp"
#include "post/personnel/ShiftState.hpp"
#include <chrono>
#include <optional>
#include <utility>
#include <vector>

namespace post {
// Плановая смена и её фактическое выполнение; перерывы записываются в хронологическом порядке.
class WorkShift {
public:
    WorkShift(const std::string& id, const std::string& employeeId, const std::string& branchId,
              std::chrono::sys_seconds scheduledStart, std::chrono::sys_seconds scheduledEnd);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& employeeId() const noexcept;
    [[nodiscard]] const std::string& branchId() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds scheduledStart() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds scheduledEnd() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> actualStart() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> actualEnd() const noexcept;
    [[nodiscard]] const std::vector<std::pair<std::chrono::sys_seconds, std::chrono::sys_seconds>>&
    breaks() const noexcept;
    [[nodiscard]] ShiftState state() const noexcept;
    void start(const Employee& employee, const std::string& targetBranchId,
               std::chrono::sys_seconds time);
    void addBreak(std::chrono::sys_seconds from, std::chrono::sys_seconds to);
    void finish(std::chrono::sys_seconds time);
    [[nodiscard]] std::chrono::seconds workedDuration(std::chrono::sys_seconds asOf) const;
    [[nodiscard]] bool overlaps(const WorkShift& other) const noexcept;
    [[nodiscard]] bool isWorkingAt(std::chrono::sys_seconds time) const;
    bool operator==(const WorkShift& other) const = default;

private:
    std::string id_;
    std::string employeeId_;
    std::string branchId_;
    std::chrono::sys_seconds scheduledStart_;
    std::chrono::sys_seconds scheduledEnd_;
    std::optional<std::chrono::sys_seconds> actualStart_;
    std::optional<std::chrono::sys_seconds> actualEnd_;
    std::vector<std::pair<std::chrono::sys_seconds, std::chrono::sys_seconds>> breaks_;
    ShiftState state_{ShiftState::Scheduled};
};
} // namespace post
