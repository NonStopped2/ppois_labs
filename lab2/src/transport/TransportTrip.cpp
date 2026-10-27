#include "post/transport/TransportTrip.hpp"
#include "post/detail/Cargo.hpp"
#include "post/detail/Time.hpp"
#include "post/exceptions/TransportUnavailableException.hpp"
#include "post/transport/TransportManifest.hpp"
#include <algorithm>
#include <type_traits>

namespace post {
// Маршрут — снимок с уже проверенной последовательностью остановок.
TransportTrip::TransportTrip(const std::string& id, const PostalRoute& route)
    : id_(detail::identifier(id)), route_(route) {}
// Ключ связывает назначенную машину и ведомость.
const std::string& TransportTrip::id() const noexcept {
    return id_;
}
// Изменение внешнего плана не перестраивает начатый рейс.
const PostalRoute& TransportTrip::route() const noexcept {
    return route_;
}
// Машина фиксируется первой загрузкой, а резервируется не позднее выезда.
const std::string& TransportTrip::vehicleId() const noexcept {
    return vehicleId_;
}
// Только рейс изменяет принадлежащие ему рабочие партии.
const std::vector<DispatchBatch>& TransportTrip::batches() const noexcept {
    return batches_;
}
// Прибытие и завершённая выгрузка — разные состояния.
TripState TransportTrip::state() const noexcept {
    return state_;
}
// Дата первого выезда не меняется на промежуточных остановках.
std::optional<std::chrono::sys_seconds> TransportTrip::actualDeparture() const noexcept {
    return actualDeparture_;
}
// До первого прибытия пуста; затем хранит последнее физическое прибытие, включая конечное.
std::optional<std::chrono::sys_seconds> TransportTrip::actualArrival() const noexcept {
    return actualArrival_;
}
// Индекс начинается с исходного отделения маршрута.
std::size_t TransportTrip::currentStopIndex() const noexcept {
    return currentStopIndex_;
}
// Нужна отдельно от прибытия: выгрузка может закончиться позднее плановой обработки.
std::optional<std::chrono::sys_seconds> TransportTrip::lastActivityAt() const noexcept {
    return lastActivityAt_;
}
// Исходный груз нужен для вместимости и ведомости, даже после выгрузки.
std::int64_t TransportTrip::totalWeightGrams() const {
    return detail::cargoWeight(batches_);
}
// Лимит числа мешков не заменяется одним лимитом массы.
std::int64_t TransportTrip::totalBags() const {
    return detail::cargoBagCount(batches_);
}
// Проверяет и ожидаемую машину, и актуальные ограничения всего подготовленного состава.
void TransportTrip::validateVehicle(const Vehicle& vehicle) const {
    const auto& allowed = route_.allowedVehicleTypes();
    if ((!vehicleId_.empty() && vehicle.id() != vehicleId_) ||
        (vehicle.state() == VehicleState::Assigned && vehicle.assignedTripId() != id_) ||
        std::find(allowed.begin(), allowed.end(), vehicle.type()) == allowed.end() ||
        totalWeightGrams() > vehicle.maxWeightGrams() || totalBags() > vehicle.maxBags()) {
        throw TransportUnavailableException("Trip exceeds vehicle capability or assignment");
    }
}
// Загрузка меняет только план рейса, физическое изъятие всех партий выполняется при depart.
void TransportTrip::loadBatch(const DispatchBatch& batch, const Vehicle& vehicle) {
    if (state_ != TripState::Planned || batch.status() != BatchStatus::Ready ||
        batch.sourceBranchId() != route_.stops().front().branchId() ||
        !route_.connects(batch.sourceBranchId(), batch.targetBranchId())) {
        throw InvalidShipmentStateException("Batch cannot join trip");
    }
    auto staged = *this;
    staged.batches_.push_back(batch);
    detail::validateCargo(staged.batches_);
    staged.validateVehicle(vehicle);
    staged.vehicleId_ = vehicle.id();
    *this = std::move(staged);
}
// Подготовка на копиях не выпускает первую партию, если последующая не прошла складские проверки.
void TransportTrip::depart(Vehicle& vehicle, const VehicleInspection& inspection,
                           const TransportManifest& manifest, PostalBranch& source,
                           ShipmentRegistry& registry, const SortingPlan& plan,
                           const std::string& employeeId, std::chrono::sys_seconds time) {
    detail::requireTime(time);
    manifest.verifyAgainst(*this);
    if (state_ != TripState::Planned || source.id() != route_.stops().front().branchId() ||
        time < route_.stops().front().earliestDeparture(manifest.issuedAt())) {
        throw InvalidShipmentStateException("Trip cannot depart here or this early");
    }
    validateVehicle(vehicle);
    vehicle.validateTrip(*this, inspection, time);
    auto staged = *this;
    auto car = vehicle;
    auto branch = source;
    auto shipments = registry;
    if (car.state() == VehicleState::Available) {
        car.assignTo(staged, inspection, time);
    }
    for (auto& batch : staged.batches_) {
        branch.transferOut(shipments, batch, plan, employeeId, time);
    }
    staged.state_ = TripState::InTransit;
    staged.actualDeparture_ = time;
    staged.lastActivityAt_ = time;
    static_assert(std::is_nothrow_move_assignable_v<PostalBranch> &&
                  std::is_nothrow_move_assignable_v<TransportTrip> &&
                  std::is_nothrow_move_assignable_v<Vehicle>);
    registry = std::move(shipments);
    source = std::move(branch);
    vehicle = std::move(car);
    *this = std::move(staged);
}
// Нельзя увезти дальше партию, предназначенную уже достигнутому отделению.
void TransportTrip::requireCurrentUnloaded() const {
    const auto& current = route_.stops()[currentStopIndex_].branchId();
    if (std::any_of(batches_.begin(), batches_.end(), [&current](const auto& batch) {
            return batch.targetBranchId() == current && batch.status() != BatchStatus::Received;
        })) {
        throw InvalidShipmentStateException("Current stop still has cargo to unload");
    }
}
// Следующая остановка, минимальное время дороги и обработка проверяются без сложения сырых минут.
void TransportTrip::arriveAt(const PostalBranch& branch, std::chrono::sys_seconds time) {
    detail::requireTime(time);
    if (state_ != TripState::InTransit) {
        throw InvalidShipmentStateException("Trip is not moving along its route");
    }
    requireCurrentUnloaded();
    const auto& next = route_.stops()[currentStopIndex_ + 1];
    const auto departure =
        currentStopIndex_ == 0
            ? *actualDeparture_
            : std::max(route_.stops()[currentStopIndex_].earliestDeparture(*actualArrival_),
                       *lastActivityAt_);
    if (branch.id() != next.branchId() ||
        time < detail::after(departure,
                             detail::secondsFromMinutes(next.travelMinutesFromPrevious()))) {
        throw InvalidShipmentStateException("Wrong or premature next stop");
    }
    ++currentStopIndex_;
    actualArrival_ = time;
    lastActivityAt_ = time;
    if (currentStopIndex_ + 1 == route_.stops().size()) {
        state_ = TripState::Arrived;
    }
}
// Все партии этого пункта принимаются одним общим снимком; вместимость учитывает их суммарный груз.
std::size_t TransportTrip::unloadFor(PostalBranch& branch, ShipmentRegistry& registry,
                                     const std::string& employeeId, const StoragePolicy& policy,
                                     std::chrono::sys_seconds time) {
    detail::requireTime(time);
    if ((state_ != TripState::InTransit && state_ != TripState::Arrived) ||
        currentStopIndex_ == 0 || branch.id() != route_.stops()[currentStopIndex_].branchId() ||
        time < *lastActivityAt_ ||
        time < route_.stops()[currentStopIndex_].earliestDeparture(*actualArrival_)) {
        throw InvalidShipmentStateException("Cargo cannot unload here or this early");
    }
    branch.authorize(employeeId, BranchOperation::ReceiveBatch, time);
    auto staged = *this;
    auto destination = branch;
    auto shipments = registry;
    std::size_t count = 0;
    for (auto& batch : staged.batches_) {
        if (batch.targetBranchId() == branch.id()) {
            if (batch.status() == BatchStatus::Received) {
                throw InvalidShipmentStateException("Stop already unloaded");
            }
            destination.receiveBatch(shipments, batch, employeeId, policy, time);
            ++count;
        }
    }
    staged.lastActivityAt_ = time;
    if (staged.state_ == TripState::Arrived) {
        staged.state_ = TripState::Completed;
    }
    registry = std::move(shipments);
    branch = std::move(destination);
    *this = std::move(staged);
    return count;
}
} // namespace post
