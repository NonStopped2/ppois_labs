#pragma once
#include "post/payments/InvoiceLine.hpp"
#include "post/shipments/Shipment.hpp"

#include <vector>

namespace post {

// PostageQuote — готовый снимок расчёта: строки начислений, тариф и срок действия.
// Привязан к трек-номеру и версии Shipment; изменение массы или услуг делает снимок устаревшим.
// Invoice хранит этот расчёт, а validateFor() проверяет его актуальность перед оплатой.
class PostageQuote {
public:
    PostageQuote(const std::string& id, const std::string& shipmentTrackingNumber,
                 std::int64_t shipmentRevision, const std::string& tariffPlanId,
                 std::chrono::sys_seconds createdAt, std::chrono::sys_seconds expiresAt,
                 std::vector<InvoiceLine> lines);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& shipmentTrackingNumber() const noexcept;
    [[nodiscard]] std::int64_t shipmentRevision() const noexcept;
    [[nodiscard]] const std::string& tariffPlanId() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds createdAt() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds expiresAt() const noexcept;
    [[nodiscard]] const std::vector<InvoiceLine>& lines() const noexcept;
    [[nodiscard]] Money total() const;
    void validateFor(const Shipment& shipment, std::chrono::sys_seconds time) const;
    bool operator==(const PostageQuote& other) const = default;

private:
    std::string id_;
    std::string shipmentTrackingNumber_;
    std::int64_t shipmentRevision_;
    std::string tariffPlanId_;
    std::chrono::sys_seconds createdAt_;
    std::chrono::sys_seconds expiresAt_;
    std::vector<InvoiceLine> lines_;
};

} // namespace post
