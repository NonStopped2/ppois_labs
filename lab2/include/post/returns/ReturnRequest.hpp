#pragma once
#include "post/personnel/Employee.hpp"
#include "post/registry/ShipmentRegistry.hpp"
#include "post/returns/ReturnStatus.hpp"
#include "post/storage/StoragePolicy.hpp"

namespace post {
class PostalBranch;
// ReturnRequest — запрос возврата отправителю: инициатор, причина, решение и даты.
// Разделяет подачу, одобрение/отказ и начало возврата через PostalBranch.
// Одобрение само не перевозит груз; исходные отправитель и получатель в Shipment не меняются.
class ReturnRequest {
public:
    explicit ReturnRequest(const std::string& id);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& shipmentId() const noexcept;
    [[nodiscard]] const std::string& initiatorId() const noexcept;
    [[nodiscard]] ReturnInitiatorKind initiatorKind() const noexcept;
    [[nodiscard]] ReturnReason reason() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> requestedAt() const noexcept;
    [[nodiscard]] ReturnStatus status() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> decidedAt() const noexcept;
    [[nodiscard]] const std::string& decisionReason() const noexcept;
    void submit(const Shipment& shipment, const Customer& customer, ReturnReason requestedReason,
                std::chrono::sys_seconds time);
    void submit(const Shipment& shipment, const Employee& employee, const PostalBranch& branch,
                ReturnReason requestedReason, std::chrono::sys_seconds time);
    void approve(const Shipment& shipment, const Employee& employee, const PostalBranch& branch,
                 const StoragePolicy& policy, std::chrono::sys_seconds time);
    void reject(const Shipment& shipment, const Employee& employee, const PostalBranch& branch,
                const std::string& rejectionReason, std::chrono::sys_seconds time);
    void beginReturn(PostalBranch& branch, ShipmentRegistry& registry, const Employee& employee,
                     std::chrono::sys_seconds time);
    bool operator==(const ReturnRequest& other) const = default;

private:
    void validateSubmission(const Shipment& shipment, std::chrono::sys_seconds time) const;
    void validateDecision(const Shipment& shipment, const Employee& employee,
                          const PostalBranch& branch, std::chrono::sys_seconds time) const;
    void recordSubmission(const Shipment& shipment, const std::string& requesterId,
                          ReturnInitiatorKind kind, ReturnReason requestedReason,
                          std::chrono::sys_seconds time);
    std::string id_;
    std::string shipmentId_;
    std::string initiatorId_;
    ReturnInitiatorKind initiatorKind_{ReturnInitiatorKind::Customer};
    ReturnReason reason_{ReturnReason::SenderRequest};
    std::optional<std::chrono::sys_seconds> requestedAt_;
    ReturnStatus status_{ReturnStatus::Draft};
    std::optional<std::chrono::sys_seconds> decidedAt_;
    std::string decisionReason_;
};
} // namespace post
