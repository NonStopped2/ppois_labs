#pragma once
#include "post/shipments/ContentItem.hpp"
#include "post/shipments/PackagingKind.hpp"
#include "post/shipments/Shipment.hpp"

#include <vector>

namespace post {

// Посылка владеет описью; после пломбирования добавление содержимого запрещено.
class Parcel : public Shipment {
public:
    Parcel(const Customer& sender, const Customer& recipient, std::int64_t weightGrams,
           const DeliveryOptions& options, const Dimensions& dimensions,
           PackagingKind packagingKind);
    [[nodiscard]] ShipmentKind kind() const noexcept override;
    [[nodiscard]] const Dimensions& dimensions() const noexcept override;
    [[nodiscard]] const std::vector<ContentItem>& contents() const noexcept;
    [[nodiscard]] PackagingKind packagingKind() const noexcept;
    [[nodiscard]] const std::string& sealId() const noexcept;
    [[nodiscard]] std::unique_ptr<Shipment> clone() const override;
    void addItem(const ContentItem& item);
    void seal(const std::string& newSealId);
    void validateForPosting(std::int64_t maxWeightGrams, const Dimensions& sizeLimit,
                            std::int64_t maxUnits) const override;

private:
    Dimensions dimensions_;
    std::vector<ContentItem> contents_;
    PackagingKind packagingKind_;
    std::string sealId_;
};

} // namespace post
