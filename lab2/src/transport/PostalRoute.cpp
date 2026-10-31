#include "post/transport/PostalRoute.hpp"
#include "post/detail/Time.hpp"
#include "post/detail/Validation.hpp"
#include <algorithm>
#include <limits>
#include <set>

// PostalRoute — план пути: упорядоченные RouteStop, предел длительности и типы машин.
// Проверяет последовательность без повторных отделений и связь пунктов в нужном направлении.
// Используется партией и рейсом; план маршрута не хранит фактическое движение машины.
namespace post {
namespace {
// Повторы и неизвестные значения не должны расширять допуск транспорта.
void validateVehicleTypes(const std::vector<VehicleType>& types) {
    std::set<VehicleType> kinds;
    std::for_each(types.begin(), types.end(), [&kinds](const auto type) {
        if ((type != VehicleType::Van && type != VehicleType::Truck) ||
            !kinds.insert(type).second) {
            throw std::invalid_argument("Invalid vehicle types");
        }
    });
}
} // namespace
// Принимает коллекции по значению и оставляет неизменяемый проверенный снимок пути.
PostalRoute::PostalRoute(const std::string& id, const std::string& name,
                         std::vector<RouteStop> stops, std::chrono::minutes maxDuration,
                         std::vector<VehicleType> allowedVehicleTypes)
    : id_(detail::identifier(id)), name_(detail::normalizeText(name)), stops_(std::move(stops)),
      maxDuration_(maxDuration), allowedVehicleTypes_(std::move(allowedVehicleTypes)) {
    validateSequence();
}
// Идентификатор связывает путь с будущими рейсами.
const std::string& PostalRoute::id() const noexcept {
    return id_;
}
// Название предназначено для человека, а не для определения направления.
const std::string& PostalRoute::name() const noexcept {
    return name_;
}
// Изменение остановок требует создания нового проверенного маршрута.
const std::vector<RouteStop>& PostalRoute::stops() const noexcept {
    return stops_;
}
// Общий лимит включает движение и обработку во всех пунктах.
std::chrono::minutes PostalRoute::maxDuration() const noexcept {
    return maxDuration_;
}
// Список допуска пригодится при назначении транспорта, но не заменяет его осмотр.
const std::vector<VehicleType>& PostalRoute::allowedVehicleTypes() const noexcept {
    return allowedVehicleTypes_;
}
// Проверяет структуру, а не только суммарное число остановок.
void PostalRoute::validateSequence() const {
    (void)detail::secondsFromMinutes(maxDuration_);
    if (!detail::hasPrintableText(name_) || stops_.size() < 2 || allowedVehicleTypes_.empty()) {
        throw std::invalid_argument("Incomplete postal route");
    }
    std::set<std::string> branches;
    for (std::size_t index = 0; index < stops_.size(); ++index) {
        if (static_cast<std::uint64_t>(stops_[index].sequence()) != index + 1 ||
            !branches.insert(stops_[index].branchId()).second) {
            throw std::invalid_argument("Invalid stop order or repeated branch");
        }
    }
    validateVehicleTypes(allowedVehicleTypes_);
    if (estimateDuration() > maxDuration_) {
        throw std::invalid_argument("Route exceeds duration limit");
    }
}
// Назначение достижимо только после отправления, не в обратном направлении и не в той же точке.
bool PostalRoute::connects(const std::string& sourceBranchId,
                           const std::string& targetBranchId) const {
    const auto source = detail::identifier(sourceBranchId);
    const auto target = detail::identifier(targetBranchId);
    const auto first = std::find_if(stops_.begin(), stops_.end(), [&source](const auto& stop) {
        return stop.branchId() == source;
    });
    const auto last = std::find_if(stops_.begin(), stops_.end(), [&target](const auto& stop) {
        return stop.branchId() == target;
    });
    return first != stops_.end() && last != stops_.end() && first < last;
}
// Проверяет каждое сложение минут, не используя непроверенный duration_cast всей суммы.
std::chrono::minutes PostalRoute::estimateDuration() const {
    std::int64_t total = 0;
    for (const auto& stop : stops_) {
        for (const auto part : {stop.travelMinutesFromPrevious(), stop.handlingMinutes()}) {
            if (part.count() > std::numeric_limits<std::int64_t>::max() - total) {
                throw std::overflow_error("Route duration overflow");
            }
            total += part.count();
        }
    }
    return std::chrono::minutes{total};
}
} // namespace post
