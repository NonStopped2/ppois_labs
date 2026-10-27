#include "post/shipments/Shipment.hpp"

#include "post/detail/Validation.hpp"
#include "post/exceptions/InvalidShipmentDataException.hpp"
#include "post/exceptions/InvalidShipmentStateException.hpp"
#include "post/exceptions/ShipmentLimitExceededException.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <tuple>
#include <utility>

namespace post {
namespace {
using Status = ShipmentStatus;
using Event = TrackingEventKind;

// Таблица перечисляет только разрешённые переходы, а не произвольные пары состояний.
Status transition(Status current, Event event) {
    static constexpr std::array rules{
        std::tuple{Status::Draft, Event::Accepted, Status::Accepted},
        std::tuple{Status::Draft, Event::Cancelled, Status::Cancelled},
        std::tuple{Status::Accepted, Event::PreparedForDispatch, Status::PreparedForDispatch},
        std::tuple{Status::PreparedForDispatch, Event::Dispatched, Status::InTransit},
        std::tuple{Status::InTransit, Event::Arrived, Status::Arrived},
        std::tuple{Status::Arrived, Event::PreparedForDispatch, Status::PreparedForDispatch},
        std::tuple{Status::Arrived, Event::OutForDelivery, Status::OutForDelivery},
        std::tuple{Status::Arrived, Event::AwaitingPickup, Status::AwaitingPickup},
        std::tuple{Status::AwaitingPickup, Event::OutForDelivery, Status::OutForDelivery},
        std::tuple{Status::AwaitingPickup, Event::Delivered, Status::Delivered},
        std::tuple{Status::OutForDelivery, Event::Delivered, Status::Delivered},
        std::tuple{Status::OutForDelivery, Event::DeliveryFailed, Status::OutForDelivery},
        std::tuple{Status::OutForDelivery, Event::Arrived, Status::Arrived},
        std::tuple{Status::Accepted, Event::ReturnStarted, Status::Returning},
        std::tuple{Status::PreparedForDispatch, Event::ReturnStarted, Status::Returning},
        std::tuple{Status::Arrived, Event::ReturnStarted, Status::Returning},
        std::tuple{Status::AwaitingPickup, Event::ReturnStarted, Status::Returning},
        std::tuple{Status::Returning, Event::PreparedForDispatch, Status::Returning},
        std::tuple{Status::Returning, Event::Dispatched, Status::Returning},
        std::tuple{Status::Returning, Event::Arrived, Status::Returning},
        std::tuple{Status::Returning, Event::Returned, Status::Returned}};
    if (event == Event::Information) {
        return current;
    }
    if (event == Event::Lost && current != Status::Draft && current != Status::Delivered &&
        current != Status::Returned && current != Status::Cancelled && current != Status::Lost) {
        return Status::Lost;
    }
    const auto found = std::find_if(rules.begin(), rules.end(), [current, event](const auto& rule) {
        return std::get<0>(rule) == current && std::get<1>(rule) == event;
    });
    if (found == rules.end()) {
        throw InvalidShipmentStateException("Forbidden shipment transition");
    }
    return std::get<2>(*found);
}

// Возврат сохраняет общий статус Returning, поэтому фазу перевозки проверяем по последнему факту.
void validateReturnLeg(const TrackingHistory& history, Event next) {
    const auto& facts = history.events();
    const auto last = std::find_if(facts.rbegin(), facts.rend(), [](const auto& fact) {
                          return fact.kind() != Event::Information;
                      })->kind();
    const bool stored = last == Event::ReturnStarted || last == Event::Arrived;
    const bool valid = (next == Event::PreparedForDispatch && stored) ||
                       (next == Event::Dispatched && last == Event::PreparedForDispatch) ||
                       (next == Event::Arrived && last == Event::Dispatched) ||
                       (next == Event::Returned && stored) || next == Event::Information ||
                       next == Event::Lost;
    if (!valid) {
        throw InvalidShipmentStateException("Invalid phase of return transportation");
    }
}

// Прибытие задаёт новое место; прочие локальные действия относятся к последнему известному.
bool needsCurrentBranch(Event kind) {
    return kind != Event::Accepted && kind != Event::Arrived && kind != Event::Information &&
           kind != Event::Cancelled && kind != Event::Lost;
}
} // namespace

// Создаёт черновик без трек-номера; номер назначит только владеющий реестр.
Shipment::Shipment(const Customer& sender, const Customer& recipient, std::int64_t weightGrams,
                   const DeliveryOptions& options)
    : sender_(sender), recipient_(recipient), weightGrams_(weightGrams), options_(options) {
    if (weightGrams_ <= 0) {
        throw InvalidShipmentDataException("Shipment weight must be positive");
    }
}
// Трек-номер остаётся пустым до регистрации.
const std::string& Shipment::trackingNumber() const noexcept {
    return trackingNumber_;
}
// Снимок отправителя не связан с последующим редактированием внешней карточки.
const Customer& Shipment::sender() const noexcept {
    return sender_;
}
// Снимок получателя используется для адреса назначения и выдачи.
const Customer& Shipment::recipient() const noexcept {
    return recipient_;
}
// Полная масса включает упаковку, а не только содержимое.
std::int64_t Shipment::weightGrams() const noexcept {
    return weightGrams_;
}
// Условия доступны только для чтения; изменение идёт через revise с новой версией.
const DeliveryOptions& Shipment::options() const noexcept {
    return options_;
}
// Статус меняется только проверяемыми доменными операциями.
ShipmentStatus Shipment::status() const noexcept {
    return status_;
}
// Нельзя напрямую дописать событие, обойдя автомат состояний.
const TrackingHistory& Shipment::history() const noexcept {
    return history_;
}
// Дата первого приёма не меняется при перевозке и возврате.
std::optional<std::chrono::sys_seconds> Shipment::acceptedAt() const noexcept {
    return acceptedAt_;
}
// Версия позволяет обнаружить устаревший расчёт стоимости.
std::int64_t Shipment::revision() const noexcept {
    return revision_;
}

// Защищает операции подготовки от изменения уже принятого отправления.
void Shipment::requireDraft() const {
    if (status_ != ShipmentStatus::Draft) {
        throw InvalidShipmentStateException("Shipment is not a draft");
    }
}
// Проверка выполняется до изменения ценовых данных или описи.
void Shipment::requireRevisionRoom() const {
    if (revision_ == std::numeric_limits<std::int64_t>::max()) {
        throw std::overflow_error("Revision exhausted");
    }
}
// Вызывается только после requireRevisionRoom, поэтому увеличение безопасно.
void Shipment::incrementRevision() noexcept {
    ++revision_;
}

// Общие лимиты задаются явно; maxUnits уточняется письмом или посылкой.
void Shipment::validateCommonLimits(std::int64_t maxWeightGrams, const Dimensions& sizeLimit,
                                    std::int64_t maxUnits) const {
    if (maxWeightGrams <= 0 || maxUnits <= 0) {
        throw std::invalid_argument("Posting limits must be positive");
    }
    options_.validateCombination(kind());
    if (weightGrams_ > maxWeightGrams || !dimensions().fitsWithin(sizeLimit)) {
        throw ShipmentLimitExceededException("Shipment exceeds weight or dimension limit");
    }
}

// Меняет влияющие на цену данные только в черновике; ошибка сохраняет прежнюю версию.
void Shipment::revise(std::int64_t newWeightGrams, const DeliveryOptions& newOptions) {
    requireDraft();
    requireRevisionRoom();
    if (newWeightGrams <= 0) {
        throw InvalidShipmentDataException("Shipment weight must be positive");
    }
    newOptions.validateCombination(kind());
    weightGrams_ = newWeightGrams;
    options_ = newOptions;
    incrementRevision();
}

// Только реестр может привязать к черновику уникальный номер и только один раз.
void Shipment::assignTrackingNumber(const std::string& number) {
    requireDraft();
    if (!trackingNumber_.empty()) {
        throw InvalidShipmentStateException("Shipment already registered");
    }
    trackingNumber_ = detail::identifier(number);
}

// Проверяет привязку локальных событий и возвращение именно в исходное отделение.
void Shipment::validateEventContext(const TrackingEvent& event) const {
    if (status_ == Status::Returning) {
        validateReturnLeg(history_, event.kind());
    }
    if (event.kind() == Event::Accepted && trackingNumber_.empty()) {
        throw InvalidShipmentStateException("Register shipment before acceptance");
    }
    if (needsCurrentBranch(event.kind()) && event.branchId() != history_.lastKnownBranch()) {
        throw InvalidShipmentStateException("Event refers to another branch");
    }
    if (event.kind() == Event::Returned) {
        const auto origin =
            std::find_if(history_.events().begin(), history_.events().end(),
                         [](const auto& fact) { return fact.kind() == Event::Accepted; });
        if (origin == history_.events().end() || origin->branchId() != event.branchId()) {
            throw InvalidShipmentStateException("Return must finish at the acceptance branch");
        }
    }
}

// Сначала готовит новую историю; статус обновляется лишь после всех проверок и копирования.
void Shipment::applyTrackingEvent(const TrackingEvent& event) {
    event.validate();
    const auto next = transition(status_, event.kind());
    validateEventContext(event);
    if (event.kind() == Event::Accepted) {
        const auto maximum = std::numeric_limits<std::int64_t>::max();
        validateForPosting(maximum, Dimensions(maximum, maximum, maximum), maximum);
    }
    auto staged = history_;
    staged.append(event);
    history_ = std::move(staged);
    status_ = next;
    if (event.kind() == Event::Accepted) {
        acceptedAt_ = event.occurredAt();
    }
}

// Отмена черновика тоже оставляет проверяемый факт в истории, а не просто меняет флаг.
void Shipment::cancelBeforeAcceptance(std::chrono::sys_seconds time, const std::string& reason) {
    requireDraft();
    const auto& facts = history_.events();
    const auto sequence = facts.empty() ? 0 : facts.back().sequence();
    if (sequence == std::numeric_limits<std::int64_t>::max()) {
        throw std::overflow_error("Event sequence exhausted");
    }
    applyTrackingEvent(TrackingEvent(sequence + 1, Event::Cancelled, time, std::nullopt, reason));
}

} // namespace post
