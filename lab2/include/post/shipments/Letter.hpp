#pragma once
#include "post/shipments/Shipment.hpp"

namespace post {

// Письмо отличается конвертом и числом листов; лимиты задаются принимающим процессом.
class Letter : public Shipment {
public:
    Letter(const Customer& sender, const Customer& recipient, std::int64_t weightGrams,
           const DeliveryOptions& options, const Dimensions& envelope, std::int64_t sheetCount,
           const std::string& contentsDescription);
    // Явная копия также подавляет неявное перемещение, способное опустошить объект реестра.
    Letter(const Letter& other) = default;
    [[nodiscard]] ShipmentKind kind() const noexcept override;
    [[nodiscard]] const Dimensions& dimensions() const noexcept override;
    [[nodiscard]] const Dimensions& envelope() const noexcept;
    [[nodiscard]] std::int64_t sheetCount() const noexcept;
    [[nodiscard]] const std::string& contentsDescription() const noexcept;
    [[nodiscard]] std::unique_ptr<Shipment> clone() const override;
    void validateForPosting(std::int64_t maxWeightGrams, const Dimensions& sizeLimit,
                            std::int64_t maxUnits) const override;

private:
    Dimensions envelope_;
    std::int64_t sheetCount_;
    std::string contentsDescription_;
};

} // namespace post
