#include "post/branches/PostalBranch.hpp"
#include "post/detail/PostalPrefixes.hpp"
#include "post/detail/ShipmentFacts.hpp"
#include "post/detail/Time.hpp"
#include "post/exceptions/DeliveryAuthorizationException.hpp"
#include "post/exceptions/PaymentException.hpp"
#include "post/exceptions/ShipmentNotFoundException.hpp"
#include "post/exceptions/StorageCapacityException.hpp"
#include <algorithm>
#include <set>
#include <type_traits>

namespace post {
namespace {
// Подпись получателя или представителя проверяется до подготовки складских изменений.
void validateClaimant(const Shipment& shipment, const Customer& claimant,
                      const IdentityDocument& document, const PickupAuthorization* authorization,
                      std::chrono::sys_seconds time) {
    const auto& principal =
        shipment.status() == ShipmentStatus::Returning ? shipment.sender() : shipment.recipient();
    if (claimant.id() == principal.id()) {
        document.validateForHolder(principal.fullName(), detail::dateAt(time));
    } else {
        if (!authorization) {
            throw DeliveryAuthorizationException("Representative needs pickup authorization");
        }
        authorization->validateFor(shipment, principal, claimant, document, time);
    }
}
// Дата партии и пломб ограничивает фактическое выбытие, но не навязывает часы операционной системы.
void requireDepartureTime(const DispatchBatch& batch, std::chrono::sys_seconds time) {
    detail::requireTime(time);
    if (time < batch.createdAt() ||
        std::any_of(batch.bags().begin(), batch.bags().end(),
                    [time](const auto& bag) { return time < *bag.sealedAt(); })) {
        throw InvalidShipmentStateException("Departure predates batch or seal");
    }
}
} // namespace

// Начальная конфигурация не загружает неизвестные отправления в обход единственного реестра.
PostalBranch::PostalBranch(const std::string& id, const std::string& name,
                           const PostalAddress& address, std::vector<StorageArea> storageAreas,
                           std::vector<Employee> employees, std::vector<WorkShift> workShifts,
                           std::vector<std::string> servedPostalPrefixes)
    : id_(detail::identifier(id)), name_(detail::normalizeText(name)), address_(address),
      storageAreas_(std::move(storageAreas)), employees_(std::move(employees)),
      workShifts_(std::move(workShifts)),
      servedPostalPrefixes_(detail::postalPrefixes(std::move(servedPostalPrefixes))) {
    validateConfiguration();
}
// Ключ отделения связывает маршруты, персонал и события.
const std::string& PostalBranch::id() const noexcept {
    return id_;
}
// Название предназначено для отображения, не для поиска полномочий.
const std::string& PostalBranch::name() const noexcept {
    return name_;
}
// Собственный адрес должен входить в обслуживаемое покрытие.
const PostalAddress& PostalBranch::address() const noexcept {
    return address_;
}
// Склад доступен только для чтения: перемещения согласует координатор.
const std::vector<StorageArea>& PostalBranch::storageAreas() const noexcept {
    return storageAreas_;
}
// Роли проверяются по принадлежащим отделению карточкам, не по внешнему заявлению.
const std::vector<Employee>& PostalBranch::employees() const noexcept {
    return employees_;
}
// Изменения смен проходят через обёртки, не позволяющие подменить назначение.
const std::vector<WorkShift>& PostalBranch::workShifts() const noexcept {
    return workShifts_;
}
// Покрытие хранится без повторов и вложенных префиксов.
const std::vector<std::string>& PostalBranch::servedPostalPrefixes() const noexcept {
    return servedPostalPrefixes_;
}
// Промежуточный узел может хранить груз вне своего конечного адресного покрытия.
bool PostalBranch::serves(const PostalAddress& destination) const {
    return std::any_of(servedPostalPrefixes_.begin(), servedPostalPrefixes_.end(),
                       [&destination](const auto& prefix) {
                           return destination.postalCode().starts_with(prefix);
                       });
}
// Находит конкретное размещение по номеру, не считая последнее известное отделение доказательством
// хранения.
const StorageEntry& PostalBranch::storageEntry(const std::string& trackingNumber) const {
    const auto number = detail::identifier(trackingNumber);
    for (const auto& area : storageAreas_) {
        for (const auto& cell : area.cells()) {
            const auto found =
                std::find_if(cell.entries().begin(), cell.entries().end(),
                             [&number](const auto& entry) { return entry.shipmentId() == number; });
            if (found != cell.entries().end()) {
                return *found;
            }
        }
    }
    throw ShipmentNotFoundException("Shipment not stored at this branch");
}
// Неизвестный сотрудник не получает полномочия через совпадение имени или роли.
const Employee& PostalBranch::employee(const std::string& employeeId) const {
    const auto key = detail::identifier(employeeId);
    const auto found = std::find_if(employees_.begin(), employees_.end(),
                                    [&key](const auto& employee) { return employee.id() == key; });
    if (found == employees_.end()) {
        throw DeliveryAuthorizationException("Unknown branch employee");
    }
    return *found;
}
// Закрытый поиск позволяет менять фактические даты, но не отдаёт наружу изменяемую карточку смены.
WorkShift& PostalBranch::shift(const std::string& shiftId) {
    const auto key = detail::identifier(shiftId);
    const auto found = std::find_if(workShifts_.begin(), workShifts_.end(),
                                    [&key](const auto& shift) { return shift.id() == key; });
    if (found == workShifts_.end()) {
        throw std::out_of_range("Unknown work shift");
    }
    return *found;
}
// Помимо роли требуется действующая собственная смена, а не только запись сотрудника в справочнике.
void PostalBranch::authorize(const std::string& employeeId, BranchOperation operation,
                             std::chrono::sys_seconds time) const {
    detail::requireTime(time);
    const auto& actor = employee(employeeId);
    actor.authorize(operation, id_);
    if (std::none_of(workShifts_.begin(), workShifts_.end(), [&actor, time](const auto& shift) {
            return shift.employeeId() == actor.id() && shift.isWorkingAt(time);
        })) {
        throw DeliveryAuthorizationException("Employee has no active work shift");
    }
}
// Начальные зоны пусты; персонал, смены и ключи не противоречат друг другу.
void PostalBranch::validateConfiguration() const {
    if (!detail::hasPrintableText(name_) || storageAreas_.empty() || !serves(address_)) {
        throw std::invalid_argument("Invalid branch configuration");
    }
    std::set<std::string> areaIds, employeeIds, shiftIds;
    for (const auto& area : storageAreas_) {
        if (!areaIds.insert(area.id()).second ||
            std::any_of(area.cells().begin(), area.cells().end(),
                        [](const auto& cell) { return !cell.entries().empty(); })) {
            throw std::invalid_argument("Branch needs distinct empty areas");
        }
    }
    if (std::any_of(employees_.begin(), employees_.end(), [this, &employeeIds](const auto& actor) {
            return actor.branchId() != id_ || !employeeIds.insert(actor.id()).second;
        })) {
        throw std::invalid_argument("Invalid branch staff");
    }
    for (auto current = workShifts_.begin(); current != workShifts_.end(); ++current) {
        (void)employee(current->employeeId());
        if (current->branchId() != id_ || !shiftIds.insert(current->id()).second ||
            std::any_of(workShifts_.begin(), current,
                        [&current](const auto& previous) { return previous.overlaps(*current); })) {
            throw std::invalid_argument("Invalid branch shifts");
        }
    }
}
// Новая смена не может изменить уже проверенную карточку или пересечься с расписанием сотрудника.
void PostalBranch::validateShift(const WorkShift& candidate) const {
    (void)employee(candidate.employeeId());
    if (candidate.branchId() != id_ ||
        std::any_of(workShifts_.begin(), workShifts_.end(), [&candidate](const auto& previous) {
            return previous.id() == candidate.id() || previous.overlaps(candidate);
        })) {
        throw std::invalid_argument("Conflicting work shift");
    }
}
// Добавляет независимо проверенное расписание; последующий push не меняет существующие смены при
// отказе.
void PostalBranch::addShift(const WorkShift& candidate) {
    validateShift(candidate);
    workShifts_.push_back(candidate);
}
// Запускает принадлежащую отделению смену с его собственной карточкой сотрудника.
void PostalBranch::startShift(const std::string& shiftId, std::chrono::sys_seconds time) {
    auto& selected = shift(shiftId);
    selected.start(employee(selected.employeeId()), id_, time);
}
// Делегирует правило перерыва, не возвращая изменяемый объект смены.
void PostalBranch::addShiftBreak(const std::string& shiftId, std::chrono::sys_seconds from,
                                 std::chrono::sys_seconds to) {
    shift(shiftId).addBreak(from, to);
}
// После завершения эта смена больше не разрешает почтовые операции.
void PostalBranch::finishShift(const std::string& shiftId, std::chrono::sys_seconds time) {
    shift(shiftId).finish(time);
}
// Сверяет складской снимок с актуальным объектом перед любым изъятием.
void PostalBranch::requireStored(const Shipment& shipment) const {
    const auto& entry = storageEntry(shipment.trackingNumber());
    if (shipment.history().lastKnownBranch() != id_ ||
        entry.storedWeightGrams() != shipment.weightGrams() ||
        entry.dimensions() != shipment.dimensions()) {
        throw InvalidShipmentStateException("Storage and registry disagree");
    }
}
// Пробует совместимые зоны на копии склада; ошибки данных не превращаются в ложную нехватку места.
void PostalBranch::placeIn(std::vector<StorageArea>& areas, const Shipment& shipment,
                           const StoragePolicy& policy, std::chrono::sys_seconds time) {
    if (std::any_of(areas.begin(), areas.end(), [&shipment](const auto& area) {
            return area.contains(shipment.trackingNumber());
        })) {
        throw InvalidShipmentStateException("Duplicate branch placement");
    }
    for (auto& area : areas) {
        try {
            area.place(shipment, policy, time);
            return;
        } catch (const StorageCapacityException&) { /* Другая зона может подойти. */
        }
    }
    throw StorageCapacityException("No suitable place at branch");
}
// Удаляет одну запись только из подготовленного склада.
void PostalBranch::takeFrom(std::vector<StorageArea>& areas, const std::string& trackingNumber) {
    const auto number = detail::identifier(trackingNumber);
    const auto area = std::find_if(areas.begin(), areas.end(),
                                   [&number](const auto& value) { return value.contains(number); });
    if (area == areas.end()) {
        throw ShipmentNotFoundException("Cargo not stored");
    }
    (void)area->take(number);
}
// Фиксация не выделяет память: после подготовки больше нет бросающих шагов между заменами.
void PostalBranch::commit(ShipmentRegistry& registry, ShipmentRegistry&& staged,
                          std::vector<StorageArea>& liveAreas,
                          std::vector<StorageArea>&& stagedAreas) noexcept {
    static_assert(std::is_nothrow_move_assignable_v<ShipmentRegistry>);
    registry = std::move(staged);
    liveAreas.swap(stagedAreas);
}
// Оплата, версия, адрес обслуживания и явные лимиты проверяются до смены состояния и размещения.
void PostalBranch::acceptShipment(ShipmentRegistry& registry, const Invoice& invoice,
                                  const std::string& trackingNumber, const std::string& employeeId,
                                  const StoragePolicy& policy, std::int64_t maxWeightGrams,
                                  const Dimensions& sizeLimit, std::int64_t maxUnits,
                                  std::chrono::sys_seconds time) {
    authorize(employeeId, BranchOperation::AcceptShipment, time);
    const auto& original = registry.at(trackingNumber);
    if (invoice.status() != InvoiceStatus::Paid || invoice.amountDue() != Money(0) ||
        time < invoice.latestActivityAt() || time > invoice.expiresAt()) {
        throw PaymentException("Acceptance requires current fully paid invoice");
    }
    invoice.quote().validateFor(original, time);
    original.validateForPosting(maxWeightGrams, sizeLimit, maxUnits);
    if (!serves(original.sender().address())) {
        throw InvalidShipmentStateException("Sender belongs to another service area");
    }
    auto staged = registry;
    auto areas = storageAreas_;
    auto& shipment = staged.at(trackingNumber);
    detail::appendFact(shipment, TrackingEventKind::Accepted, time, id_);
    placeIn(areas, shipment, policy, time);
    commit(registry, std::move(staged), storageAreas_, std::move(areas));
}
// Вся партия покидает ячейки вместе с фактами подготовки/выбытия или не меняет ничего.
void PostalBranch::transferOut(ShipmentRegistry& registry, DispatchBatch& batch,
                               const SortingPlan& plan, const std::string& employeeId,
                               std::chrono::sys_seconds time) {
    authorize(employeeId, BranchOperation::Dispatch, time);
    if (batch.sourceBranchId() != id_ || batch.status() != BatchStatus::Ready ||
        plan.branchId() != id_) {
        throw InvalidShipmentStateException("Batch unavailable for branch dispatch");
    }
    requireDepartureTime(batch, time);
    auto staged = registry;
    auto areas = storageAreas_;
    auto prepared = batch;
    for (const auto& bag : prepared.bags()) {
        for (const auto& [cargoId, weight] : bag.shipmentIds()) {
            auto& shipment = staged.at(cargoId);
            requireStored(shipment);
            if (shipment.weightGrams() != weight ||
                !plan.groupForDispatch({std::cref(shipment)}).contains(batch.targetBranchId())) {
                throw InvalidShipmentStateException("Cargo disagrees with batch direction");
            }
            if (detail::lastOperationalFact(shipment) != TrackingEventKind::PreparedForDispatch) {
                detail::appendFact(shipment, TrackingEventKind::PreparedForDispatch, time, id_);
            }
            detail::appendFact(shipment, TrackingEventKind::Dispatched, time, id_);
            takeFrom(areas, cargoId);
        }
    }
    prepared.markDispatched();
    static_assert(std::is_nothrow_move_assignable_v<DispatchBatch>);
    commit(registry, std::move(staged), storageAreas_, std::move(areas));
    batch = std::move(prepared);
}
// Приём использует один подготовленный склад для всех номеров, учитывая суммарную вместимость.
void PostalBranch::receiveBatch(ShipmentRegistry& registry, DispatchBatch& batch,
                                const std::string& employeeId, const StoragePolicy& policy,
                                std::chrono::sys_seconds time) {
    authorize(employeeId, BranchOperation::ReceiveBatch, time);
    if (batch.targetBranchId() != id_ || batch.status() != BatchStatus::InTransit) {
        throw InvalidShipmentStateException("Batch is not in transit to this branch");
    }
    auto staged = registry;
    auto areas = storageAreas_;
    auto prepared = batch;
    for (const auto& bag : prepared.bags()) {
        for (const auto& [cargoId, weight] : bag.shipmentIds()) {
            auto& shipment = staged.at(cargoId);
            if (shipment.weightGrams() != weight) {
                throw InvalidShipmentStateException("Cargo weight disagrees with manifest");
            }
            detail::appendFact(shipment, TrackingEventKind::Arrived, time, id_);
            if (shipment.status() == ShipmentStatus::Arrived &&
                serves(shipment.recipient().address())) {
                detail::appendFact(shipment, TrackingEventKind::AwaitingPickup, time, id_);
            }
            placeIn(areas, shipment, policy, time);
        }
    }
    prepared.markReceived(*this, staged, time);
    commit(registry, std::move(staged), storageAreas_, std::move(areas));
    batch = std::move(prepared);
}
// Выдача проверяет физическое размещение, конечное покрытие и документ до изъятия.
void PostalBranch::issueShipment(ShipmentRegistry& registry, const std::string& trackingNumber,
                                 const std::string& employeeId, const Customer& claimant,
                                 const IdentityDocument& document, std::chrono::sys_seconds time,
                                 const PickupAuthorization* authorization) {
    authorize(employeeId, BranchOperation::IssueShipment, time);
    const auto& original = registry.at(trackingNumber);
    requireStored(original);
    const bool returning = original.status() == ShipmentStatus::Returning;
    const auto& principal = returning ? original.sender() : original.recipient();
    if (!serves(principal.address())) {
        throw DeliveryAuthorizationException("Pickup belongs to another service area");
    }
    validateClaimant(original, claimant, document, authorization, time);
    auto staged = registry;
    auto areas = storageAreas_;
    auto& shipment = staged.at(trackingNumber);
    if (shipment.status() == ShipmentStatus::Arrived) {
        detail::appendFact(shipment, TrackingEventKind::AwaitingPickup, time, id_);
    }
    detail::appendFact(shipment,
                       returning ? TrackingEventKind::Returned : TrackingEventKind::Delivered, time,
                       id_);
    takeFrom(areas, trackingNumber);
    commit(registry, std::move(staged), storageAreas_, std::move(areas));
}
// Начало возврата не извлекает груз из ячейки до настоящего отправления обратной партии.
void PostalBranch::beginReturn(ShipmentRegistry& registry, const std::string& trackingNumber,
                               const std::string& employeeId, std::chrono::sys_seconds time) {
    authorize(employeeId, BranchOperation::ApproveReturn, time);
    const auto& original = registry.at(trackingNumber);
    requireStored(original);
    detail::requireReturnable(original);
    auto staged = registry;
    detail::appendFact(staged.at(trackingNumber), TrackingEventKind::ReturnStarted, time, id_,
                       "Разрешён возврат отправителю");
    registry = std::move(staged);
}
} // namespace post
