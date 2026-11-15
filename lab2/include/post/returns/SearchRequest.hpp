#pragma once
#include "post/branches/PostalBranch.hpp"
#include "post/returns/SearchStatus.hpp"
#include <set>
#include <tuple>

namespace post {
// SearchRequest — розыск: снимок истории, ожидаемые отделения и результаты физических проверок.
// Подтверждает нахождение либо утрату; утрата согласованно отражается в реестре и складе.
// Новое событие делает снимок устаревшим; для признания утраты нужны проверки и минимальный срок.
class SearchRequest {
public:
    // Результат физической проверки: дата, найдено ли отправление, пояснение сотрудника.
    using Finding = std::tuple<std::chrono::sys_seconds, bool, std::string>;
    explicit SearchRequest(const std::string& id);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& shipmentId() const noexcept;
    [[nodiscard]] const std::string& applicantCustomerId() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> openedAt() const noexcept;
    [[nodiscard]] const std::string& lastKnownBranchId() const noexcept;
    [[nodiscard]] const std::vector<std::string>& expectedBranchIds() const noexcept;
    [[nodiscard]] const std::set<std::string>& checkedBranchIds() const noexcept;
    [[nodiscard]] const std::map<std::string, Finding>& findings() const noexcept;
    [[nodiscard]] SearchStatus status() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> closedAt() const noexcept;
    [[nodiscard]] std::int64_t openedSequence() const noexcept;
    void open(const Shipment& shipment, const Customer& applicant, const PostalRoute& route,
              std::chrono::sys_seconds time);
    void recordBranchCheck(const PostalBranch& branch, const std::string& employeeId, bool found,
                           const std::string& note, std::chrono::sys_seconds time);
    void resolveFound(const Shipment& shipment, const PostalBranch& branch,
                      std::chrono::sys_seconds time);
    void closeAsLost(PostalBranch& branch, ShipmentRegistry& registry,
                     const std::string& employeeId, std::chrono::seconds minimumAge,
                     std::chrono::sys_seconds time);
    bool operator==(const SearchRequest& other) const = default;

private:
    void requireSnapshot(const Shipment& shipment) const;
    void requireResolutionTime(std::chrono::sys_seconds time) const;
    std::string id_;
    std::string shipmentId_;
    std::string applicantCustomerId_;
    std::optional<std::chrono::sys_seconds> openedAt_;
    std::string lastKnownBranchId_;
    std::vector<std::string> expectedBranchIds_;
    std::set<std::string> checkedBranchIds_;
    std::map<std::string, Finding> findings_;
    SearchStatus status_{SearchStatus::Draft};
    std::optional<std::chrono::sys_seconds> closedAt_;
    std::int64_t openedSequence_{0};
};
} // namespace post
