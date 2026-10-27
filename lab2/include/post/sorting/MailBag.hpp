#pragma once
#include "post/registry/ShipmentRegistry.hpp"
#include "post/sorting/SortingPlan.hpp"
#include <map>

namespace post {
class PostalBranch;
// Состав мешка — снимки номеров и массы, не второе владение отправлениями; пломба фиксирует состав.
class MailBag {
public:
    MailBag(const std::string& id, std::int64_t maxWeightGrams,
            const std::string& destinationBranchId);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] std::int64_t maxWeightGrams() const noexcept;
    [[nodiscard]] const std::string& destinationBranchId() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> sealedAt() const noexcept;
    [[nodiscard]] const std::string& sealId() const noexcept;
    [[nodiscard]] const std::map<std::string, std::int64_t>& shipmentIds() const noexcept;
    [[nodiscard]] std::int64_t totalWeightGrams() const noexcept;
    void add(const Shipment& shipment, const PostalBranch& destination, const SortingPlan& plan);
    void remove(const std::string& trackingNumber);
    void seal(const std::string& requestedSealId, std::chrono::sys_seconds time,
              const ShipmentRegistry& registry);
    bool operator==(const MailBag& other) const = default;

private:
    std::string id_;
    std::int64_t maxWeightGrams_;
    std::string destinationBranchId_;
    std::optional<std::chrono::sys_seconds> sealedAt_;
    std::string sealId_;
    std::map<std::string, std::int64_t> shipmentIds_;
    std::int64_t totalWeightGrams_{0};
};
} // namespace post
