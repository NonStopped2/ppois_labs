#pragma once
#include "post/payments/Invoice.hpp"
#include "post/returns/CompensationClaim.hpp"
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace post {
// CashRegister — сеанс кассы отделения: остаток наличных, платежи, возвраты и компенсации.
// Проверяет полномочия и время; карточный оборот не меняет физический остаток наличных.
// Изменения кассы и счёта/требования готовит на копиях и принимает только после успеха.
class CashRegister {
public:
    using CompensationJournal = std::map<std::string, std::pair<Money, std::chrono::sys_seconds>>;
    CashRegister(const std::string& id, const std::string& branchId);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& branchId() const noexcept;
    [[nodiscard]] const Money& openingBalance() const noexcept;
    [[nodiscard]] const Money& cashBalance() const noexcept;
    [[nodiscard]] const std::vector<Payment>& payments() const noexcept;
    [[nodiscard]] const std::vector<Refund>& refunds() const noexcept;
    [[nodiscard]] const CompensationJournal& compensations() const noexcept;
    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> openedAt() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> closedAt() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds latestActivityAt() const noexcept;
    void open(Money opening, const PostalBranch& branch, const std::string& employeeId,
              std::chrono::sys_seconds time);
    void acceptPayment(Invoice& invoice, const Payment& payment, const Shipment& shipment,
                       const PostalBranch& branch, const std::string& employeeId);
    void refundPayment(Invoice& invoice, const Refund& refund, const PostalBranch& branch,
                       const std::string& employeeId);
    void payCompensation(CompensationClaim& claim, const PostalBranch& branch,
                         const std::string& employeeId, std::chrono::sys_seconds time);
    [[nodiscard]] bool reconcile(Money countedCash) const noexcept;
    void close(Money countedCash, const PostalBranch& branch, const std::string& employeeId,
               std::chrono::sys_seconds time);
    bool operator==(const CashRegister& other) const = default;

private:
    void requireAccess(const PostalBranch& branch, const std::string& employeeId,
                       std::chrono::sys_seconds time) const;
    std::string id_;
    std::string branchId_;
    Money openingBalance_;
    Money cashBalance_;
    std::vector<Payment> payments_;
    std::vector<Refund> refunds_;
    CompensationJournal compensations_;
    bool isOpen_{false};
    std::optional<std::chrono::sys_seconds> openedAt_;
    std::optional<std::chrono::sys_seconds> closedAt_;
};
} // namespace post
