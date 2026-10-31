#pragma once
#include "post/core/Money.hpp"
#include "post/returns/CompensationStatus.hpp"
#include "post/returns/SearchRequest.hpp"
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace post {
// CompensationClaim — требование возмещения утраты/повреждения: заявитель, доказательства и суммы.
// Для утраты проверяет завершённый розыск; решение ограничено запросом и заданным лимитом.
// CashRegister проводит выплату и помечает требование Paid; это не возврат тарифа через Refund.
class CompensationClaim {
public:
    explicit CompensationClaim(const std::string& id);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& shipmentId() const noexcept;
    [[nodiscard]] const std::string& claimantCustomerId() const noexcept;
    [[nodiscard]] const Money& claimedAmount() const noexcept;
    [[nodiscard]] const Money& approvedAmount() const noexcept;
    [[nodiscard]] CompensationReason reason() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> submittedAt() const noexcept;
    [[nodiscard]] CompensationStatus status() const noexcept;
    [[nodiscard]] const std::vector<std::string>& evidenceReferences() const noexcept;
    [[nodiscard]] const std::string& decisionReason() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> paidAt() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> decidedAt() const noexcept;
    void submit(const Shipment& shipment, const Customer& claimant, Money amount,
                CompensationReason reason, std::vector<std::string> evidence,
                std::chrono::sys_seconds time);
    [[nodiscard]] Money assess(const Shipment& shipment, Money limit,
                               const SearchRequest* search = nullptr) const;
    void approve(const Shipment& shipment, const PostalBranch& branch,
                 const std::string& employeeId, Money amount, Money limit,
                 std::chrono::sys_seconds time, const SearchRequest* search = nullptr);
    void reject(const PostalBranch& branch, const std::string& employeeId,
                const std::string& explanation, std::chrono::sys_seconds time);
    void markPaid(Money amount, std::chrono::sys_seconds time);
    bool operator==(const CompensationClaim& other) const = default;

private:
    std::string id_;
    std::string shipmentId_;
    std::string claimantCustomerId_;
    Money claimedAmount_;
    Money approvedAmount_;
    CompensationReason reason_{CompensationReason::Lost};
    std::optional<std::chrono::sys_seconds> submittedAt_;
    CompensationStatus status_{CompensationStatus::Draft};
    std::vector<std::string> evidenceReferences_;
    std::string decisionReason_;
    std::optional<std::chrono::sys_seconds> paidAt_;
    std::optional<std::chrono::sys_seconds> decidedAt_;
};
} // namespace post
