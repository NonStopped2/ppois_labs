#pragma once

#include "post/core/Customer.hpp"
#include "post/core/Dimensions.hpp"
#include "post/shipments/DeliveryOptions.hpp"
#include "post/shipments/ShipmentStatus.hpp"
#include "post/tracking/TrackingHistory.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace post {
class ShipmentRegistry;

// Общие данные и автомат состояний отправления; письмо и посылка уточняют проверку продукта.
// Клиенты и история принадлежат объекту по значению, внешние карточки их не изменяют.
class Shipment {
public:
    virtual ~Shipment() = default;
    [[nodiscard]] const std::string& trackingNumber() const noexcept;
    [[nodiscard]] const Customer& sender() const noexcept;
    [[nodiscard]] const Customer& recipient() const noexcept;
    [[nodiscard]] std::int64_t weightGrams() const noexcept;
    [[nodiscard]] const DeliveryOptions& options() const noexcept;
    [[nodiscard]] ShipmentStatus status() const noexcept;
    [[nodiscard]] const TrackingHistory& history() const noexcept;
    [[nodiscard]] std::optional<std::chrono::sys_seconds> acceptedAt() const noexcept;
    [[nodiscard]] std::int64_t revision() const noexcept;
    [[nodiscard]] virtual ShipmentKind kind() const noexcept = 0;
    [[nodiscard]] virtual const Dimensions& dimensions() const noexcept = 0;
    [[nodiscard]] virtual std::unique_ptr<Shipment> clone() const = 0;
    virtual void validateForPosting(std::int64_t maxWeightGrams, const Dimensions& sizeLimit,
                                    std::int64_t maxUnits) const = 0;

    void applyTrackingEvent(const TrackingEvent& event);
    void cancelBeforeAcceptance(std::chrono::sys_seconds time, const std::string& reason);
    void revise(std::int64_t newWeightGrams, const DeliveryOptions& newOptions);

protected:
    // Копия-снимок допустима, но присваивание не должно подменять личность объекта в реестре.
    Shipment(const Shipment& other) = default;
    Shipment& operator=(const Shipment& other) = delete;
    Shipment(const Customer& sender, const Customer& recipient, std::int64_t weightGrams,
             const DeliveryOptions& options);
    void requireDraft() const;
    void requireRevisionRoom() const;
    void incrementRevision() noexcept;
    void validateCommonLimits(std::int64_t maxWeightGrams, const Dimensions& sizeLimit,
                              std::int64_t maxUnits) const;

private:
    friend class ShipmentRegistry;
    void assignTrackingNumber(const std::string& number);
    void validateEventContext(const TrackingEvent& event) const;

    std::string trackingNumber_;
    Customer sender_;
    Customer recipient_;
    std::int64_t weightGrams_;
    DeliveryOptions options_;
    ShipmentStatus status_{ShipmentStatus::Draft};
    TrackingHistory history_;
    std::optional<std::chrono::sys_seconds> acceptedAt_;
    std::int64_t revision_{0};
};

} // namespace post
