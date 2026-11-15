#include "post/transport/Vehicle.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/TransportUnavailableException.hpp"
#include "post/transport/TransportTrip.hpp"
#include <algorithm>

// Vehicle — транспорт: регистрационный номер, тип, грузоподъёмность и предел числа мешков.
// Проверяет вместимость, маршрут и VehicleInspection перед назначением на TransportTrip.
// Хранит ID назначения, а не копию рейса; освобождение проверяет завершение этого рейса.
namespace post {
// Положительные лимиты и известный вид транспорта проверяются до резервирования.
Vehicle::Vehicle(const std::string& id, const std::string& registrationNumber,
                 std::int64_t maxWeightGrams, std::int64_t maxBags, VehicleType type)
    : id_(detail::identifier(id)), registrationNumber_(detail::identifier(registrationNumber)),
      maxWeightGrams_(maxWeightGrams), maxBags_(maxBags), type_(type) {
    if (maxWeightGrams_ <= 0 || maxBags_ <= 0 ||
        (type_ != VehicleType::Van && type_ != VehicleType::Truck)) {
        throw std::invalid_argument("Invalid vehicle data");
    }
}
// Ключ машины отличен от регистрационного номера.
const std::string& Vehicle::id() const noexcept {
    return id_;
}
// Формат государственного справочника модель не проверяет.
const std::string& Vehicle::registrationNumber() const noexcept {
    return registrationNumber_;
}
// Лимит относится ко всему грузу рейса.
std::int64_t Vehicle::maxWeightGrams() const noexcept {
    return maxWeightGrams_;
}
// Количество мешков ограничивается независимо от массы.
std::int64_t Vehicle::maxBags() const noexcept {
    return maxBags_;
}
// Назначение изменяется только согласованными операциями.
VehicleState Vehicle::state() const noexcept {
    return state_;
}
// Свободная машина не имеет фиктивного идентификатора рейса.
const std::optional<std::string>& Vehicle::assignedTripId() const noexcept {
    return assignedTripId_;
}
// Вид участвует в допуске на маршрут.
VehicleType Vehicle::type() const noexcept {
    return type_;
}
// Повторная проверка нужна при фактическом старте, даже если машину резервировали заранее.
void Vehicle::validateTrip(const TransportTrip& trip, const VehicleInspection& inspection,
                           std::chrono::sys_seconds time) const {
    inspection.validateFor(*this, time);
    const auto& allowed = trip.route().allowedVehicleTypes();
    if (trip.state() != TripState::Planned || trip.vehicleId() != id_ || trip.batches().empty() ||
        trip.totalWeightGrams() > maxWeightGrams_ || trip.totalBags() > maxBags_ ||
        std::find(allowed.begin(), allowed.end(), type_) == allowed.end()) {
        throw TransportUnavailableException("Vehicle cannot operate this trip");
    }
}
// Резервирование готовит строку до записи состояния; занятая машина не перехватывается другим
// рейсом.
void Vehicle::assignTo(const TransportTrip& trip, const VehicleInspection& inspection,
                       std::chrono::sys_seconds time) {
    if (state_ != VehicleState::Available) {
        throw TransportUnavailableException("Vehicle already assigned");
    }
    validateTrip(trip, inspection, time);
    std::optional<std::string> assignment(trip.id());
    assignedTripId_.swap(assignment);
    state_ = VehicleState::Assigned;
}
// Завершение требует тот же рейс и выгрузку всех его партий.
void Vehicle::releaseFrom(const TransportTrip& trip) {
    if (state_ != VehicleState::Assigned || assignedTripId_ != trip.id() ||
        trip.vehicleId() != id_ || trip.state() != TripState::Completed) {
        throw TransportUnavailableException("Vehicle cannot be released from this trip");
    }
    assignedTripId_.reset();
    state_ = VehicleState::Available;
}
} // namespace post
