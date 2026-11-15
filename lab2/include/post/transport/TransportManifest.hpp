#pragma once
#include "post/transport/TransportTrip.hpp"
#include <map>

namespace post {
// TransportManifest — транспортная ведомость: снимок ID партий, пломб и общей массы рейса.
// buildFor() создаёт её по подготовленному TransportTrip; verifyAgainst() сверяет состав.
// Не владеет отправлениями и не меняется вместе с рейсом: нужна для контроля перед выездом.
class TransportManifest {
public:
    [[nodiscard]] static TransportManifest
    buildFor(const std::string& id, const TransportTrip& trip, std::chrono::sys_seconds time);
    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const std::string& tripId() const noexcept;
    [[nodiscard]] std::chrono::sys_seconds issuedAt() const noexcept;
    [[nodiscard]] const std::vector<std::string>& batchIds() const noexcept;
    [[nodiscard]] const std::map<std::string, std::string>& seals() const noexcept;
    [[nodiscard]] std::int64_t declaredWeightGrams() const noexcept;
    void verifyAgainst(const TransportTrip& trip) const;
    bool operator==(const TransportManifest& other) const = default;

private:
    TransportManifest(const std::string& id, const TransportTrip& trip,
                      std::chrono::sys_seconds time);
    std::string id_;
    std::string tripId_;
    std::chrono::sys_seconds issuedAt_;
    std::vector<std::string> batchIds_;
    std::map<std::string, std::string> seals_;
    std::int64_t declaredWeightGrams_;
};
} // namespace post
