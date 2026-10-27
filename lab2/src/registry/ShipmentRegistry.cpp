#include "post/registry/ShipmentRegistry.hpp"
#include "post/detail/Validation.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace post {
namespace {
// Только завершённые состояния допускают архивирование.
bool completed(ShipmentStatus status) {
    return status == ShipmentStatus::Delivered || status == ShipmentStatus::Returned ||
           status == ShipmentStatus::Cancelled || status == ShipmentStatus::Lost;
}
// Поздние поясняющие сообщения не изменяют дату завершения отправления.
std::chrono::sys_seconds completedAt(const Shipment& shipment) {
    const auto& events = shipment.history().events();
    const auto found = std::find_if(events.rbegin(), events.rend(), [](const auto& event) {
        return event.kind() != TrackingEventKind::Information;
    });
    return found->occurredAt();
}
// Проверяет допустимость enum без молчаливого пустого результата поиска.
void requireStatus(ShipmentStatus status) {
    if (status < ShipmentStatus::Draft || status > ShipmentStatus::Lost) {
        throw std::invalid_argument("Unknown shipment status");
    }
}
} // namespace

// Счётчик можно восстановить при создании реестра; ноль зарезервирован для исчерпания.
ShipmentRegistry::ShipmentRegistry(const std::string& trackingPrefix, std::int64_t firstSequence)
    : nextSequence_(firstSequence), trackingPrefix_(detail::identifier(trackingPrefix)) {
    if (firstSequence <= 0) {
        throw std::invalid_argument("First tracking sequence must be positive");
    }
}
// В отличие от копирования указателей, clone создаёт независимые объекты.
ShipmentRegistry::Store ShipmentRegistry::cloneStore(const Store& original) {
    Store result;
    for (const auto& [number, shipment] : original) {
        result.emplace(number, shipment->clone());
    }
    return result;
}
// Архив и активные записи копируются одинаково глубоко.
ShipmentRegistry::ShipmentRegistry(const ShipmentRegistry& other)
    : shipments_(cloneStore(other.shipments_)),
      archivedShipments_(cloneStore(other.archivedShipments_)), nextSequence_(other.nextSequence_),
      trackingPrefix_(other.trackingPrefix_) {}
// Обмен готовых значений не выделяет память и используется для сильной гарантии присваивания.
void ShipmentRegistry::swap(ShipmentRegistry& other) noexcept {
    shipments_.swap(other.shipments_);
    archivedShipments_.swap(other.archivedShipments_);
    std::swap(nextSequence_, other.nextSequence_);
    trackingPrefix_.swap(other.trackingPrefix_);
}
// Сначала строит полную копию; неудача копирования не затрагивает исходный реестр.
ShipmentRegistry& ShipmentRegistry::operator=(const ShipmentRegistry& other) {
    if (this != &other) {
        auto copy = ShipmentRegistry(other);
        swap(copy);
    }
    return *this;
}
// Номер и копия готовятся до вставки; счётчик меняется лишь после успешного размещения.
std::string ShipmentRegistry::registerShipment(const Shipment& prototype,
                                               std::int64_t maxWeightGrams,
                                               const Dimensions& sizeLimit, std::int64_t maxUnits) {
    if (nextSequence_ == 0) {
        throw std::overflow_error("Tracking numbers exhausted");
    }
    if (!prototype.trackingNumber().empty() || prototype.status() != ShipmentStatus::Draft) {
        throw InvalidShipmentStateException("Only an unregistered draft can be registered");
    }
    prototype.validateForPosting(maxWeightGrams, sizeLimit, maxUnits);
    auto number = trackingPrefix_ + "-" + std::to_string(nextSequence_);
    auto owned = prototype.clone();
    owned->assignTrackingNumber(number);
    shipments_.emplace(number, std::move(owned));
    nextSequence_ =
        nextSequence_ == std::numeric_limits<std::int64_t>::max() ? 0 : nextSequence_ + 1;
    return number;
}
// Изменяемая ссылка даёт право вызвать только проверяемые операции Shipment.
Shipment& ShipmentRegistry::at(const std::string& trackingNumber) {
    // Исходный реестр неконстантный, поэтому снятие const безопасно.
    // Общая реализация позволяет не дублировать поиск.
    return const_cast<Shipment&>(std::as_const(*this).at(trackingNumber));
}
// Поиск охватывает архив: перенос не делает трек-номер неизвестным.
const Shipment& ShipmentRegistry::at(const std::string& trackingNumber) const {
    const auto number = detail::identifier(trackingNumber);
    for (const auto* store : {&shipments_, &archivedShipments_}) {
        const auto found = store->find(number);
        if (found != store->end()) {
            return *found->second;
        }
    }
    throw ShipmentNotFoundException("Unknown tracking number");
}
// Выбирает состояние и среди текущих, и среди архивных отправлений.
std::vector<std::reference_wrapper<const Shipment>>
ShipmentRegistry::findByStatus(ShipmentStatus status) const {
    requireStatus(status);
    std::vector<std::reference_wrapper<const Shipment>> result;
    for (const auto* store : {&shipments_, &archivedShipments_}) {
        for (const auto& [number, shipment] : *store) {
            if (shipment->status() == status) {
                result.emplace_back(*shipment);
            }
        }
    }
    return result;
}
// Сопоставляет идентификаторы клиента, а не изменяемые контактные данные карточки.
std::vector<std::reference_wrapper<const Shipment>>
ShipmentRegistry::findByCustomer(const Customer& customer) const {
    std::vector<std::reference_wrapper<const Shipment>> result;
    for (const auto* store : {&shipments_, &archivedShipments_}) {
        for (const auto& [number, shipment] : *store) {
            if (shipment->sender().id() == customer.id() ||
                shipment->recipient().id() == customer.id()) {
                result.emplace_back(*shipment);
            }
        }
    }
    return result;
}
// Перенос узлов map сохраняет адреса объектов и не копирует их; незавершённые остаются активны.
std::size_t ShipmentRegistry::archiveCompletedBefore(std::chrono::sys_seconds cutoff) {
    detail::requireTime(cutoff);
    std::vector<std::string> eligible;
    for (const auto& [number, shipment] : shipments_) {
        if (completed(shipment->status()) && completedAt(*shipment) < cutoff) {
            eligible.push_back(number);
        }
    }
    std::for_each(eligible.begin(), eligible.end(), [this](const auto& number) {
        archivedShipments_.insert(shipments_.extract(number));
    });
    return eligible.size();
}
// Количество активных записей не включает архив.
std::size_t ShipmentRegistry::activeCount() const noexcept {
    return shipments_.size();
}
// Количество архивных записей доступно для проверки переноса.
std::size_t ShipmentRegistry::archivedCount() const noexcept {
    return archivedShipments_.size();
}
// Нулевой следующий номер означает исчерпание пространства, не новый первый номер.
std::int64_t ShipmentRegistry::nextSequence() const noexcept {
    return nextSequence_;
}
// Префикс входит в генерируемый идентификатор и не меняется после создания реестра.
const std::string& ShipmentRegistry::trackingPrefix() const noexcept {
    return trackingPrefix_;
}

} // namespace post
