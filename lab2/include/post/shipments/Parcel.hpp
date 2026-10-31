#pragma once
#include "post/shipments/ContentItem.hpp"
#include "post/shipments/PackagingKind.hpp"
#include "post/shipments/Shipment.hpp"

#include <vector>

namespace post {

// Parcel — вид Shipment для посылки: габариты, упаковка, опись ContentItem и пломба.
// Проверяет согласованность содержимого с массой, ценностью и условиями отправления.
// После пломбирования нельзя добавлять позиции; clone() сохраняет полный тип Parcel.
class Parcel : public Shipment {
public:
    Parcel(const Customer& sender, const Customer& recipient, std::int64_t weightGrams,
           const DeliveryOptions& options, const Dimensions& dimensions,
           PackagingKind packagingKind);
    // Снимки копируются, а не забирают опись и пломбу у зарегистрированного оригинала.
    Parcel(const Parcel& other) = default;
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
